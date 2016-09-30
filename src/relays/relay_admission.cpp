// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "teleportnode/teleportnode.h"

#include "log.h"
#define LOG_CATEGORY "relay_admission.cpp"


void AddRelayDutyForBatch(uint160 credit_hash)
{
    log_ << "AddRelayDutyForBatch(): " << credit_hash << "\n";
    RelayJoinMessage join_msg(credit_hash);
    join_msg.Sign();
    log_ << "join is: " << join_msg;
    relaydata[join_msg.GetHash160()]["is_mine"] = true;
    relaydata[join_msg.relay_pubkey]["is_mine"] = true;
    relaydata[credit_hash]["is_mine"] = true;
    relaydata[join_msg.relay_pubkey]["credit_hash"] = join_msg.credit_hash;
    teleportnode.relayhandler.BroadcastMessage(join_msg);
}


void DoScheduledJoinCheck(uint160 join_hash)
{
    if (relaydata[join_hash]["cancelled"])
        return;
    RelayJoinMessage join_msg = msgdata[join_hash]["join"];
    if (relaydata[join_msg.relay_pubkey]["ejected"])
        return;
    if (relaydata[join_hash]["complaint_received"] &&
        !relaydata[join_hash]["complaint_check_completed"])
    {
        teleportnode.scheduler.Schedule("join_check", join_hash,
                                      GetTimeMicros() + COMPLAINT_WAIT_TIME);
        return;
    }

    teleportnode.relayhandler.AcceptJoin(join_msg);
}

void DoScheduledJoinComplaintCheck(uint160 complaint_hash)
{
    uint160 join_hash;
    string type = msgdata[complaint_hash]["type"];
    RelayJoinMessage join;

    if (type == "join_complaint_successor")
    {
        JoinComplaintFromSuccessor complaint = msgdata[complaint_hash][type];
        join_hash = complaint.join_hash;
    
    }
    else if (type == "join_complaint_executor")
    {
        JoinComplaintFromExecutor complaint = msgdata[complaint_hash][type];
        join_hash = complaint.join_hash;
    }
    
    if (relaydata[complaint_hash]["refuted"])
    {
        relaydata[join_hash]["complaint_check_completed"] = true;
        return;
    }

    join = msgdata[join_hash]["join"];
    teleportnode.relayhandler.EjectApplicant(join.relay_pubkey);
    relaydata[join_hash]["complaint_check_completed"] = true;
}

void DoScheduledFutureSuccessorComplaintCheck(uint160 complaint_hash)
{
    throw NotImplementedException("DoScheduledFutureSuccessorComplaintCheck");
}



/******************
 *  RelayHandler
 */

    bool RelayHandler::JoinMessageIsCompatibleWithState(RelayJoinMessage& msg)
    {
        log_ << "JoinMessageIsCompatibleWithState(): " << msg;
        
        if (relaydata[msg.credit_hash].HasProperty("pubkey"))
        {
            log_ << "pubkey already assigned to that credit\n";
            return false;
        }
        if (!InMainChain(msg.credit_hash))
        {
            log_ << "queuing join message from credit not in main chain:"
                 << msg.credit_hash << "\n"
                 << "calendar is: " << teleportnode.calendar;
            QueueJoin(msg);
            return false;
        }

        return true;
    }

    bool RelayHandler::ValidateRelayJoinMessage(RelayJoinMessage& msg)
    {
        bool signature_verified = msg.VerifySignature();
        if (!signature_verified)
        {
            log_ << "signature verification failed\n";
            return false;
        }

        return msg.relay_number < INHERITANCE_START 
                || ValidateJoinSuccessionSecrets(msg);
    }

    bool RelayHandler::ValidateJoinSuccessionSecrets(RelayJoinMessage& msg)
    {
        if (msg.distributed_succession_secret.Relays()
            != teleportnode.RelayState().Executors(msg.relay_number))
        {
            log_ << "wrong executors!\n"
                 << msg.distributed_succession_secret.Relays() << " vs "
                 << teleportnode.RelayState().Executors(msg.relay_number) << "\n";
            return false;
        }
        if (!msg.distributed_succession_secret.Validate())
        {
            log_ << "bad distributed secret";
            return false;
        }
        return true;
    }

    void RelayHandler::QueueJoin(RelayJoinMessage join)
    {
        uint160 join_hash = join.GetHash160();
        vector<uint160> queued_joins
            = relaydata[join.credit_hash]["queued_joins"];
        queued_joins.push_back(join_hash);
        relaydata[join.credit_hash]["queued_joins"] = queued_joins;
    }

    void RelayHandler::RecordDownStreamApplicant(Point applicant,
                                                 Point downstream_applicant)
    {
        vector<Point> downstream_applicants
            = relaydata[applicant]["downstream_applicants"];
        downstream_applicants.push_back(downstream_applicant);
        relaydata[applicant]["downstream_applicants"] = downstream_applicants;

        log_ << "downstream applicants of " << applicant
             << " are now " << downstream_applicants << "\n";
    }

    void RelayHandler::HandleQueuedJoins(uint160 credit_hash)
    {
        std::vector<uint160> queued_joins
            = relaydata[credit_hash]["queued_joins"];

        foreach_(const uint160& join_hash, queued_joins)
        {
            log_ << "Handling queued join " << join_hash << "\n";
            RelayJoinMessage join = msgdata[join_hash]["join"];
            HandleRelayJoinMessage(join);
        }

        relaydata.EraseProperty(credit_hash, "queued_joins");
    }

    void RelayHandler::HandleRelayJoinMessage(RelayJoinMessage join)
    {
        log_ << "relayhandler: received join message: " << join;;
        uint160 join_hash = join.GetHash160();
        if (relaydata[join_hash]["handled"])
            return;

        if (!creditdata[join.credit_hash].HasProperty("mined_credit"))
        {
            log_ << "Don't have this credit - requesting\n";
            NodeId peer_id = msgdata[join_hash]["peer"];
            CNode *peer = GetPeer(peer_id);

            if (!peer)
                return;

            uint160 hash = join.credit_hash;
            teleportnode.downloader.datahandler.RequestBatch(hash, peer);
            QueueJoin(join);
        
            return;
        }

        log_ << "previous_mined_credit_hash is "
             << teleportnode.previous_mined_credit_hash << "\n";

        if (!ValidateRelayJoinMessage(join))
        {
            log_ << "failed to validate message\n";
            return;
        }

        StoreHash(join_hash);

        relaydata[join.relay_pubkey]["join_msg_hash"] = join_hash;

        if (!JoinMessageIsCompatibleWithState(join))
        {
            log_ << "Join Message Is Not Compatible With State\n";
            return;
        }
        
        RelayJoinMessage preceding_join
            = msgdata[join.preceding_relay_join_hash]["join"];

        if (join.relay_number != preceding_join.relay_number + 1)
        {
            log_ << "Wrong relay number: " << join.relay_number << " after "
                 << preceding_join.relay_number << "\n";
            return;
        }

        relaydata[join_hash].Location("valid_join_handle_time") = GetTimeMicros();
        
        if (latest_valid_join_hash == join.preceding_relay_join_hash)
            latest_valid_join_hash = join_hash;
        else
            log_ << "lastest valid join is " << latest_valid_join_hash
                 << " not " << join.preceding_relay_join_hash << "\n";

        if (!relay_chain.state.relays.count(preceding_join.relay_pubkey)
            && join.relay_number > 1)
        {
            log_ << "preceding_relay " << preceding_join.relay_pubkey 
                 << " with join message " << preceding_join
                 << " is not in state. queueing\n";
            relaydata[join.relay_pubkey]["join"] = join;
            LOCK(relay_handler_mutex);
                RecordDownStreamApplicant(preceding_join.relay_pubkey,
                                          join.relay_pubkey);
            return;
        }

        teleportnode.scheduler.Schedule("join_check", join_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);

        log_ << "scheduled join check for approximately "
             << GetTimeMicros() + COMPLAINT_WAIT_TIME << "\n";
        
        relaydata[join_hash]["handled"] = true;

#ifndef _PRODUCTION_BUILD
        if (join.relay_number == 13)
        {
            log_ << "Doing test succession for relay 9.\n";
            DoSuccession(teleportnode.RelayState().RelaysByNumber()[9]);
        }
#endif
    }

    void RelayHandler::HandleDownStreamApplicants(Point applicant)
    {
        vector<Point> applicants = GetDownstream(applicant);

        foreach_(Point applicant_, applicants)
        {
            log_ << "Handling applicant " << applicant_
                 << " downstream of " << applicant << "\n";
            RelayJoinMessage join = relaydata[applicant_]["join"];
            HandleRelayJoinMessage(join);
        }
        relaydata[applicant]["downstream_applicants"] = vector<Point>();
    }

    RelayJoinMessage RelayHandler::GetLatestValidJoin()
    {
        CLocationIterator<> join_scanner
            = relaydata.LocationIterator("valid_join_handle_time");
        uint160 join_hash;
        uint64_t timestamp;
        LOCK(relay_handler_mutex);
        {
            join_scanner.Seek(GetTimeMicros());
            join_scanner.GetPreviousObjectAndLocation(join_hash, timestamp);
        }
        RelayJoinMessage join = msgdata[join_hash]["join"];
        return join;
    }

    void RelayHandler::AcceptJoin(RelayJoinMessage msg)
    {
        log_ << "AcceptJoin():\n"
             << "join is: " << msg;

        if (!InMainChain(msg.credit_hash))
        {
            log_ << msg.credit_hash << " is not in main chain - failing!\n";
            return;
        }

        if (msg.relay_number <= relay_chain.state.latest_relay_number)
        {
            log_ << "relay number " << msg.relay_number 
                 << " is taken; latest is "
                 << relay_chain.state.latest_relay_number << "\n";
            HandleDownStreamApplicants(msg.relay_pubkey);
            return;
        }

        uint160 message_hash = msg.GetHash160();

        log_ << "relayhandler: accepting join message "
             << message_hash << "\n";

        msgdata[msg.credit_hash]["accepted_join"] = message_hash;
        
        StoreJoinMessage(msg);

        LOCK(relay_handler_mutex);
            relay_chain.HandleMessage(message_hash);

        if (keydata[msg.relay_pubkey].HasProperty("privkey"))
            relaydata[msg.relay_pubkey].Location("my_relays")
                = GetTimeMicros();

        
        if (msg.relay_number == relay_number_of_my_future_successor)
            HandleMyFutureSuccessor(msg);

        if (msg.relay_number >= INHERITANCE_START)
            HandleSuccessorSecrets(msg);
        HandleExecutorSecrets(msg);
        teleportnode.pit.HandleRelayMessage(message_hash);
        log_ << "relay " << msg.relay_number << " has joined\n";

        log_ << "AcceptJoin(): handling downstream_applicants\n"
             << "namely " << GetDownstream(msg.relay_pubkey) << "\n";
        HandleDownStreamApplicants(msg.relay_pubkey);
    }

    void RelayHandler::HandleExecutorSecrets(RelayJoinMessage msg)
    {
        foreach_(const Point& executor, 
                 msg.distributed_succession_secret.relays)
        {
            if (keydata[executor].HasProperty("privkey"))
                HandleExecutorSecret(msg, executor);
        }
    }

    void RelayHandler::HandleExecutorSecret(RelayJoinMessage msg, 
                                            Point executor)
    {
        CBigNum privkey = keydata[executor]["privkey"];

        DistributedSuccessionSecret dist_secret
            = msg.distributed_succession_secret;

        int32_t position = dist_secret.RelayPosition(executor);
        Point point = dist_secret.point_values[position];

        CBigNum secret_xor_shared_secret
            = dist_secret.secrets_xor_shared_secrets[position];

        CBigNum shared_secret = Hash(privkey * point);

        CBigNum secret = secret_xor_shared_secret ^ shared_secret;

        if (Point(SECP256K1, secret) != point)
        {
            JoinComplaintFromExecutor complaint(msg.GetHash160(), position);
            BroadcastMessage(complaint);
            return;
        }
        
        keydata[point]["privkey"] = secret;
    }

    void RelayHandler::EjectApplicant(Point applicant)
    {
        log_ << "Ejecting applicant: " << applicant << "\n";
        if (!relaydata[applicant]["is_applicant"])
        {
            log_ << applicant << " is not an applicant!\n";
            return;
        }
        relaydata[applicant]["ejected"] = true;

        LOCK(relay_handler_mutex);
        {
            vector<Point> downstream_applicants = GetDownstream(applicant);

            vector<Point> need_to_reapply;

            foreach_(Point downstream_applicant, downstream_applicants)
            {
                relaydata[downstream_applicant]["ejected"] = true;
                if (relaydata[downstream_applicant]["is_mine"])
                    need_to_reapply.push_back(downstream_applicant);
            }

            foreach_(Point my_applicant, need_to_reapply)
                ReJoin(my_applicant);

            latest_valid_join_hash = GetLatestValidJoin().GetHash160();
        }
    }

    void RelayHandler::ReJoin(Point applicant)
    {
        uint160 credit_hash = relaydata[applicant]["credit_hash"];
        AddRelayDutyForBatch(credit_hash);
    }

    vector<Point> RelayHandler::GetDownstream(Point applicant)
    {
        vector<Point> downstream;
        GetDownstream(applicant, downstream);
        return downstream;
    }

    void RelayHandler::GetDownstream(Point applicant, 
                                     vector<Point>& downstream)
    {
        vector<Point> downstream_applicants
            = relaydata[applicant]["downstream_applicants"];
        foreach_(Point downstream_applicant, downstream_applicants)
        {
            downstream.push_back(downstream_applicant);
            GetDownstream(downstream_applicant, downstream);
        }
    }

    void RelayHandler::HandleJoinComplaintFromSuccessor(
        JoinComplaintFromSuccessor complaint)
    {
        uint160 complaint_hash = complaint.GetHash160();

        if (!complaint.VerifySignature())
        {
            should_forward = false;
            return;
        }
        
        if (relaydata[complaint.join_hash]["is_mine"])
        {
            RefutationOfJoinComplaintFromSuccessor refutation(complaint_hash);
            BroadcastMessage(refutation);
        }

        teleportnode.scheduler.Schedule("join_complaint", complaint_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void RelayHandler::HandleRefutationOfJoinComplaintFromSuccessor(
        RefutationOfJoinComplaintFromSuccessor refutation)
    {
        if (!refutation.VerifySignature() || !refutation.Validate())
        {
            should_forward = false;
            return;
        }
        relaydata[refutation.complaint_hash]["refuted"] = true;
    }


    void RelayHandler::StoreJoinMessage(RelayJoinMessage msg)
    {
        uint160 message_hash = msg.GetHash160();

        relaydata[msg.credit_hash]["pubkey"] = msg.relay_pubkey;
        relaydata[msg.relay_pubkey]["join_msg_hash"] = message_hash;
        relaydata[message_hash]["received"] = true;
    }

    bool RelayHandler::HandleSuccessorSecrets(RelayJoinMessage msg)
    {
        if (msg.relay_number % FUTURE_SUCCESSOR_FREQUENCY == 1)
            return false;
        log_ << "HandleSuccessorSecrets()\n";
        RelayJoinMessage my_join_msg
            = msgdata[msg.preceding_relay_join_hash]["join"];
        CBigNum privkey = keydata[my_join_msg.relay_pubkey]["privkey"];
        if (privkey == 0)
        {
            log_ << "I'm not the successor\n";
            return false;
        }
        CBigNum shared_secret = Hash(privkey * msg.successor_secret_point);
        CBigNum successor_secret
            = msg.successor_secret_xor_shared_secret ^ shared_secret;
#ifndef _PRODUCTION_BUILD
        log_ << "privkey: " << privkey << "\n"
             << "successor_secret: " << successor_secret << "\n"
             << "successor_secret_point: " << msg.successor_secret_point << "\n"
             << "shared_secret: " << shared_secret << "\n"
             << "successor_secret_xor_shared_secret: "
             << msg.successor_secret_xor_shared_secret << "\n";
#endif  
        if (Point(SECP256K1, successor_secret) != msg.successor_secret_point)
        {
            JoinComplaintFromSuccessor complaint(msg.GetHash160());
            BroadcastMessage(complaint);
            return false;
        }
        log_ << "HandleSuccessorSecrets: Successfully got secret for "
             << msg.successor_secret_point << "\n";
#ifndef _PRODUCTION_BUILD
        log_ << "namely " << successor_secret << "\n";
#endif
        keydata[msg.successor_secret_point]["privkey"] = successor_secret;
        return true;
    }

    void RelayHandler::HandleMyFutureSuccessor(RelayJoinMessage 
                                               successor_join_msg)
    {
        FutureSuccessorSecretMessage msg(credit_hash_awaiting_successor,
                                         successor_join_msg.relay_pubkey);
        BroadcastMessage(msg);
    }

    bool RelayHandler::ValidateFutureSuccessorSecretMessage(
        FutureSuccessorSecretMessage msg)
    {
        RelayJoinMessage successor_join_msg, predecessor_join_msg;
        successor_join_msg = msgdata[msg.successor_join_hash]["join"];
        uint32_t successor_number = successor_join_msg.relay_number;

        uint160 predecessor_join_hash = msg.JoinHash();
        predecessor_join_msg = msgdata[predecessor_join_hash]["join"];

        uint32_t predecessor_number = predecessor_join_msg.relay_number;

        return (predecessor_number % FUTURE_SUCCESSOR_FREQUENCY) == 1 &&
               (successor_number == predecessor_number
                                    + 2 * FUTURE_SUCCESSOR_FREQUENCY - 1) &&
                msg.VerifySignature();
    }

    void RelayHandler::HandleFutureSuccessorSecretMessage(FutureSuccessorSecretMessage msg)
    {
        if (!ValidateFutureSuccessorSecretMessage(msg))
            return;
        uint160 msg_hash = msg.GetHash160();
        StoreHash(msg_hash);

        RelayJoinMessage join = msgdata[msg.successor_join_hash]["join"];
        if (keydata[join.VerificationKey()].HasProperty("privkey"))
        {
            CBigNum privkey = keydata[join.VerificationKey()]["privkey"];
            Point point = join.successor_secret_point;
            CBigNum shared_secret = Hash(privkey * point);

            CBigNum successor_secret
                = msg.successor_secret_xor_shared_secret ^ shared_secret;
            if (Point(SECP256K1, successor_secret) != point)
            {
                ComplaintFromFutureSuccessor complaint(msg.GetHash160());
                BroadcastMessage(complaint);
                return;
            }
            keydata[point]["privkey"] = successor_secret;
        }

        relay_chain.HandleMessage(msg_hash);
        teleportnode.pit.HandleRelayMessage(msg_hash);
    }

    void RelayHandler::ScheduleFutureSuccessor(uint160 credit_hash, 
                                               uint32_t relay_number)
    {
        credit_hash_awaiting_successor = credit_hash;
        relay_number_of_my_future_successor
            = relay_number + 2 * FUTURE_SUCCESSOR_FREQUENCY - 1;
    }

    bool RelayHandler::IsTooLateToComplain(uint160 join_hash)
    {
        return relay_chain.state.join_hash_to_relay.count(join_hash);
    }

    void RelayHandler::HandleJoinComplaintFromExecutor(
        JoinComplaintFromExecutor complaint)
    {
        log_ << "Handling:" << complaint;
        if (IsTooLateToComplain(complaint.join_hash))
        {
            log_ << "complaint is too late\n";
            return;
        }
        if (!complaint.VerifySignature())
        {
            should_forward = false;
            return;
        }
        relaydata[complaint.join_hash]["complaint_received"] = true;
        teleportnode.scheduler.Schedule("join_complaint", 
                                    complaint.GetHash160(),
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void RelayHandler::HandleRefutationOfJoinComplaintFromExecutor(
        RefutationOfJoinComplaintFromExecutor refutation)
    {
        if (refutation.VerifySignature() && refutation.Validate())
            relaydata[refutation.complaint_hash]["refuted"] = true;
    }

    void RelayHandler::HandleComplaintFromFutureSuccessor(
        ComplaintFromFutureSuccessor complaint)
    {
        teleportnode.scheduler.Schedule("future_successor_complaint",
                                      complaint.GetHash160(),
                                      GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void RelayHandler::HandleRefutationOfComplaintFromFutureSuccessor(
        RefutationOfComplaintFromFutureSuccessor refutation)
    {
        if (refutation.VerifySignature() && refutation.Validate())
            relaydata[refutation.complaint_hash]["refuted"] = true;
    }
/*
 *  RelayHandler
 ******************/
