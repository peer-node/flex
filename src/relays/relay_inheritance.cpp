#include "teleportnode/teleportnode.h"

#include "log.h"
#define LOG_CATEGORY "relay_inheritance.cpp"

#define SUCCESSION_WAIT_TIME (30 * 1000 * 1000)


RelayJoinMessage GetJoinMessage(Point relay)
{
    uint160 join_msg_hash = relaydata[relay]["join_msg_hash"];
    return msgdata[join_msg_hash]["join"];
}

bool CheckSuccessionMessage(SuccessionMessage msg)
{
    log_ << "CheckSuccessionMessage()\n";
    RelayJoinMessage join = msgdata[msg.dead_relay_join_hash]["join"];
    Point relay = join.relay_pubkey;
    vector<Point> executors = teleportnode.relayhandler.Executors(relay);
    Point successor = teleportnode.relayhandler.Successor(relay);
    if (teleportnode.RelayState().GetJoinHash(successor) != msg.successor_join_hash)
    {
        log_ << "bad successor join hash: "
             << msg.successor_join_hash << " vs "
             << teleportnode.RelayState().GetJoinHash(successor) << "\n";
        return false;
    }
    if (!VectorContainsEntry(executors, msg.executor))
    {
        log_ << "Wrong executor: " << msg.executor << " vs "
             << executors << "\n";
        return false;
    }
    if (!msg.VerifySignature())
    {
        log_ << "bad signature\n";
        return false;
    }
    if (!relaydata[relay]["dead"])
    {
        log_ << "This relay isn't dead. queueing succession message\n";
        vector<SuccessionMessage> msgs = relaydata[relay]["queued_succession_msgs"];
        msgs.push_back(msg);
        relaydata[relay]["queued_succession_msgs"] = msgs;
        return false;
    }
    log_ << "succession message ok\n";
    return true;
}

void HandleQueuedSuccessionMessages(Point dead_relay)
{
    vector<SuccessionMessage> msgs = relaydata[dead_relay]["queued_succession_msgs"];
    foreach_(SuccessionMessage msg, msgs)
    {
        teleportnode.relayhandler.HandleSuccessionMessage(msg);
    }
    relaydata.EraseProperty(dead_relay, "queued_succession_msgs");
}

void RegisterSuccessionMessage(SuccessionMessage msg)
{
    RelayJoinMessage join = msgdata[msg.dead_relay_join_hash]["join"];
    Point relay = join.relay_pubkey;
    Point executor = msg.executor;
    vector<Point> remaining_executors = relaydata[relay]["awaited"];
    EraseEntryFromVector(executor, remaining_executors);
    relaydata[relay]["awaited"] = remaining_executors;
}

uint32_t IncrementAndCountSuccessionSecrets(Point dead_relay)
{
    uint32_t num_secrets = relaydata[dead_relay]["num_secrets"];
    num_secrets += 1;
    relaydata[dead_relay]["num_secrets"] = num_secrets;
    return num_secrets;
}

bool RecoverSecretKey(RelayJoinMessage join)
{
    DistributedSuccessionSecret dist_secret
        = join.distributed_succession_secret;
    CBigNum privkey;
    std::vector<CBigNum> secrets;
    bool cheated;
    std::vector<int32_t> cheaters;

    foreach_(const Point& point, dist_secret.point_values)
    {
        CBigNum secret = keydata[point]["privkey"];
        secrets.push_back(secret);
    }

    generate_private_key_from_N_secret_parts(SECP256K1,
                                             privkey,
                                             secrets,
                                             dist_secret.point_values,
                                             secrets.size(),
                                             cheated,
                                             cheaters);
    CBigNum successor_secret
         = keydata[join.successor_secret_point]["privkey"];

    CBigNum relay_privkey = (successor_secret + privkey) 
                                % Point(SECP256K1, 0).Modulus();
#ifndef _PRODUCTION_BUILD
    log_ << "InheritDuties(): combining " << successor_secret
         << " with " << privkey << " to get " << relay_privkey << "\n";
#endif
    if (Point(SECP256K1, relay_privkey) != join.relay_pubkey)
    {
        log_ << "InheritDuties(): failed: recovered key " << relay_privkey
             << " gives point " << Point(SECP256K1, relay_privkey)
             << " instead of " << join.relay_pubkey << "\n";
        return false;
    }
    else
    {
        log_ << "InheritDuties(): Successfully recovered privkey for "
             << join.relay_pubkey << "\n";
    }
    keydata[join.relay_pubkey]["privkey"] = relay_privkey;
    relaydata[join.credit_hash]["is_mine"] = true;
    return true;
}

void InheritDuties(RelayJoinMessage join)
{
    Point relay = join.relay_pubkey;
    log_ << "InheritDuties(): " << relay << "\n";
 
    if (!RecoverSecretKey(join))
    {
        log_ << "Failed to recover key for " << join.relay_pubkey << "\n";
        return;
    }

    RelayState state = teleportnode.RelayState();
    Point dead_predecessor = state.DeadPredecessor(relay);

    while (!dead_predecessor.IsAtInfinity())
    {
        log_ << "recoving duties of dead predecessor "
             << dead_predecessor << "\n";

        uint160 join_hash = state.GetJoinHash(dead_predecessor);
        InheritDuties(msgdata[join_hash]["join"]);
        dead_predecessor = state.DeadPredecessor(dead_predecessor);
    }

    Point successor = state.Successor(relay);
    HandleInheritedTasks(successor);
}

DistributedSuccessionSecret GetDistributedSuccessionSecret(Point relay)
{
    uint160 join_message_hash = relaydata[relay]["join_hash"];

    RelayJoinMessage msg = msgdata[join_message_hash]["join"];
    return msg.distributed_succession_secret;
}

void HandleInheritedTrade(uint160 accept_commit_hash)
{
    tradedata[accept_commit_hash]["i_have_secret"] = true;
    tradedata[accept_commit_hash]["i_sent_secrets"] = false;
    tradedata[accept_commit_hash]["bid_secret_sent"] = false;
    tradedata[accept_commit_hash]["ask_secret_sent"] = false;

    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    teleportnode.tradehandler.CheckForMyRelaySecrets(accept_commit);

    if (tradedata[accept_commit_hash]["cancelled"])
        SendSecrets(accept_commit_hash, BACKWARD);
    else
        SendSecrets(accept_commit_hash, FORWARD);
}

void HandleInheritedSuccession(uint160 dead_relay_join_hash)
{
    RelayJoinMessage join = msgdata[dead_relay_join_hash]["join"];
    teleportnode.relayhandler.HandleExecutorSecrets(join);
    DoSuccession(join.relay_pubkey);
}

void HandleInheritedDepositRequest(uint160 request_hash)
{
    depositdata[request_hash]["inherited"] = true;
    uint160 encoding_credit_hash;
    encoding_credit_hash = depositdata[request_hash]["encoding_credit_hash"];
    teleportnode.deposit_handler.HandleEncodedRequest(request_hash,
                                                  encoding_credit_hash);
}

void HandleInheritedDepositTransfer(uint160 transfer_hash)
{
    depositdata[transfer_hash]["inherited"] = true;
    DepositTransferMessage transfer = depositdata[transfer_hash]["transfer"];
    teleportnode.deposit_handler.HandleDepositTransferMessage(transfer);
}

void HandleInheritedWithdrawalRequest(uint160 part_msg_hash)
{
    DepositAddressPartMessage part_msg;
    part_msg = msgdata[part_msg_hash]["deposit_part"];

    teleportnode.deposit_handler.CheckAndSaveSharesFromAddressPart(
                                                        part_msg,
                                                        part_msg.position);

    uint160 address_request_hash = part_msg.address_request_hash;
    Point address = depositdata[address_request_hash]["deposit_address"];

    uint160 withdraw_request_hash = depositdata[address]["withdraw_request"];

    depositdata[withdraw_request_hash]["inherited"] = true;
    uint32_t parts_received;
    parts_received = depositdata[withdraw_request_hash]["parts_received"];

    for (uint32_t position = 0; position < SECRETS_PER_DEPOSIT; position++)
    {
        if (!(parts_received & (1 << position)))
        {
            SendBackupWithdrawalsFromPartMessages(withdraw_request_hash,
                                                  position);
        }
    }
}

void HandleInheritedTask(uint160 task_hash)
{
    string type = msgdata[task_hash]["type"];
    if (type == "accept_commit")
        HandleInheritedTrade(task_hash);
    else if (type == "join")
        HandleInheritedSuccession(task_hash);
    else if (type == "deposit_request")
        HandleInheritedDepositRequest(task_hash);
    else if (type == "transfer")
        HandleInheritedDepositTransfer(task_hash);
    else if (type == "withdraw_request")
        HandleInheritedWithdrawalRequest(task_hash);
}

void HandleInheritedTasks(Point successor)
{
    CLocationIterator<Point> task_scanner;
    uint160 task_hash;
    uint64_t timestamp;

    task_scanner = taskdata.LocationIterator(successor);
    task_scanner.SeekStart();

    while (task_scanner.GetNextObjectAndLocation(task_hash, timestamp))
    {
        HandleInheritedTask(task_hash);
        taskdata.RemoveFromLocation(successor, timestamp);
        task_scanner = taskdata.LocationIterator(successor);
        task_scanner.SeekStart();
    }
}

bool ValidateRelayLeaveMessage(RelayLeaveMessage msg)
{
    if (msg.successor != 
        teleportnode.relayhandler.relay_chain.state.Successor(msg.relay_leaving))
        return false;
    if (relaydata[msg.relay_leaving]["dead"])
        return false;
    return msg.VerifySignature();
}

void TransferTasks(Point dead_relay, Point successor)
{
    uint64_t now = GetTimeMicros();
    if (!successor)
        return;

    CLocationIterator<Point> task_scanner;
    task_scanner = taskdata.LocationIterator(dead_relay);
    uint160 task_hash;
    uint64_t task_time;

    task_scanner.Seek(now);

    while (task_scanner.GetPreviousObjectAndLocation(task_hash, task_time))
    {
        taskdata[task_hash].Location(successor) = task_time;
        taskdata.RemoveFromLocation(dead_relay, task_time);
        task_scanner = taskdata.LocationIterator(dead_relay);
        task_scanner.Seek(now);
    }    
}

#define NO_SUCCESSION_AFTER (24 * 60 * 60) // seconds

void LeaveRelayPool()
{
    uint64_t now = GetTimeMicros();
    CLocationIterator<> duty_scanner;
    duty_scanner = relaydata.LocationIterator("my_relays");
    
    duty_scanner.Seek(now);

    uint64_t join_time;
    Point relay_key;

    while (duty_scanner.GetPreviousObjectAndLocation(relay_key, join_time))
    {
        if (now - join_time > NO_SUCCESSION_AFTER * 1e6)
            break;
        RelayLeaveMessage msg(relay_key);
        teleportnode.relayhandler.BroadcastMessage(msg);
        relaydata.RemoveFromLocation("my_relays", join_time);

        duty_scanner = relaydata.LocationIterator("my_relays");
        duty_scanner.Seek(now);
    }
}

void DoScheduledSuccessionComplaintCheck(uint160 complaint_hash)
{
    if (relaydata[complaint_hash]["refuted"])
        return;
    SuccessionComplaint complaint;
    complaint = msgdata[complaint_hash]["succession_complaint"];

    SuccessionMessage msg;
    msg = msgdata[complaint.succession_msg_hash]["succession"];

    Point bad_relay = msg.VerificationKey();
    DoSuccession(bad_relay);
}

void DoScheduledSuccessionCheck(uint160 join_hash)
{
    log_ << "DoScheduledSuccessionCheck(): " << join_hash << "\n";
    RelayJoinMessage join = msgdata[join_hash]["join"];
    Point relay = join.relay_pubkey;
    vector<Point> remaining_executors = relaydata[relay]["awaited"];
    
    foreach_(Point non_responder, remaining_executors)
        if (!relaydata[non_responder]["dead"])
            DoSuccession(non_responder);
}

void SendSuccessionSecrets(Point dead_relay)
{
    uint64_t time_of_death = relaydata[dead_relay]["time_of_death"];
    Point successor = teleportnode.relayhandler.Successor(dead_relay);
    vector<Point> executors = teleportnode.relayhandler.Executors(dead_relay);

    foreach_(const Point& executor, executors)
    {
        taskdata[executor].Location(dead_relay) = time_of_death;
        if (keydata[executor].HasProperty("privkey"))
        {
            SuccessionMessage succession_msg(dead_relay, executor);
            teleportnode.relayhandler.BroadcastMessage(succession_msg);
        }
    }
}

void DoSuccession(Point key_to_recover, Point dead_keyholder)
{
    log_ << "DoSuccession() recover: " << key_to_recover 
         << " through succession of " << dead_keyholder << "\n";

    uint160 join_hash = teleportnode.RelayState().GetJoinHash(dead_keyholder);

    RelayJoinMessage join = msgdata[join_hash]["join"];
    if (join.relay_number < INHERITANCE_START)
    {
        log_ << "Relay number too low for inheritance: "
             << join.relay_number << " vs " << INHERITANCE_START << "\n";
        return;
    }

    if (relaydata[dead_keyholder]["dead"])
    {
        SendSuccessionSecrets(dead_keyholder);
        return;
    }

    relaydata[dead_keyholder]["time_of_death"] = GetTimeMicros();
    relaydata[dead_keyholder]["dead"] = true;
    keydata.EraseProperty(dead_keyholder, "privkey");

    HandleQueuedSuccessionMessages(dead_keyholder);

    Point successor = teleportnode.relayhandler.Successor(dead_keyholder);
    vector<Point> executors = teleportnode.relayhandler.Executors(dead_keyholder);

    relaydata[dead_keyholder]["awaited"] = executors;
    
    TransferTasks(dead_keyholder, successor);

    SendSuccessionSecrets(dead_keyholder);

    teleportnode.scheduler.Schedule("succession_check", join_hash,
                                GetTimeMicros() + 3 * COMPLAINT_WAIT_TIME);
}

void DoSuccession(Point key_to_recover)
{
    Point dead_keyholder = key_to_recover;
    while (relaydata[dead_keyholder]["dead"])
    {
        dead_keyholder = teleportnode.relayhandler.Successor(dead_keyholder);
    }
    DoSuccession(key_to_recover, dead_keyholder);
}


/***************
 *  RelayHandler
 */

    void RelayHandler::HandleRelayLeaveMessage(RelayLeaveMessage msg)
    {
        if (!ValidateRelayLeaveMessage(msg))
            return;

        DistributedSuccessionSecret secret
            = GetDistributedSuccessionSecret(msg.relay_leaving);

        uint160 msg_hash = msg.GetHash160();
        StoreHash(msg_hash);
        
        relay_chain.HandleMessage(msg_hash);

        relaydata[msg.relay_leaving]["dead"] = true;

        if (keydata[msg.successor].HasProperty("privkey"))
        {
            CBigNum privkey = keydata[msg.successor]["privkey"];
            CBigNum shared_secret = Hash(privkey * secret.PubKey());
            CBigNum succession_secret
                = msg.succession_secret_xor_shared_secret ^ shared_secret;

            if (secret.PubKey() != Point(SECP256K1, succession_secret))
            {
                RelayLeaveComplaint complaint(msg_hash);
                BroadcastMessage(complaint);
            }
            keydata[secret.PubKey()]["privkey"] = succession_secret;
        }
        TransferTasks(msg.relay_leaving, msg.successor);
    }

    void RelayHandler::HandleSuccessionMessage(SuccessionMessage msg)
    {
        log_ << "HandleSuccessionMessage(): " << msg;
        if (!CheckSuccessionMessage(msg))
        {
            log_ << "bad succession message\n";
            should_forward = false;
            return;
        }
        uint160 msg_hash = msg.GetHash160();
        StoreHash(msg_hash);
        RegisterSuccessionMessage(msg);

        RelayJoinMessage join = msgdata[msg.dead_relay_join_hash]["join"];

        DistributedSuccessionSecret dist_secret
            = join.distributed_succession_secret;

        int32_t position = dist_secret.RelayPosition(msg.executor);

        relay_chain.HandleMessage(msg_hash);
        teleportnode.pit.HandleRelayMessage(msg_hash);

        RelayJoinMessage successor_join 
            = msgdata[msg.successor_join_hash]["join"];
        Point successor = successor_join.relay_pubkey;
        log_ << "successor is " << successor << "\n";
        if (!keydata[successor].HasProperty("privkey"))
            return;
        CBigNum privkey = keydata[successor]["privkey"];

        Point point = dist_secret.point_values[position];

        CBigNum shared_secret = Hash(privkey * point);
        CBigNum executor_secret
            = msg.succession_secret_xor_shared_secret ^ shared_secret;
#ifndef _PRODUCTION_BUILD
        log_ << "HandleSuccessionMessage: point is " << point << "\n"
             << "privkey is " << privkey << "\n"
             << "shared_secret is " << shared_secret << "\n"
             << "executor_secret is " << executor_secret << "\n";
#endif
        if (Point(SECP256K1, executor_secret) != point)
        {
            log_ << "bad executor secret: " << executor_secret << "\n"
                 << " gives point " << Point(SECP256K1, executor_secret)
                 << " not " << point << "\n"
                 << " - complaining\n";
            SuccessionComplaint complaint(msg_hash);
            BroadcastMessage(complaint);
            return;
        }

        keydata[point]["privkey"] = executor_secret;
        
        uint32_t num_secrets;
        num_secrets = IncrementAndCountSuccessionSecrets(join.relay_pubkey);
        log_ << "number of succession secrets received: " 
             << num_secrets << "\n";
        if (num_secrets >= SUCCESSION_SECRETS_NEEDED)
        {
            log_ << "inheriting duties\n";
            InheritDuties(join);
        }
    }

    void RelayHandler::HandleSuccessionComplaint(SuccessionComplaint complaint)
    {
        if (!complaint.VerifySignature())
        {
            should_forward = false;
            return;
        }
        teleportnode.scheduler.Schedule("succession_complaint", 
                                      complaint.GetHash160(),
                                      GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void RelayHandler::HandleRefutationOfSuccessionComplaint(
        RefutationOfSuccessionComplaint refutation)
    {
        if (!refutation.VerifySignature() || !refutation.Validate())
        {
            should_forward = false;
            return;
        }
        relaydata[refutation.complaint_hash]["refuted"] = true;
    }

/*
 *  RelayHandler
 ****************/
