// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#include "teleportnode/teleportnode.h"

#include "log.h"
#define LOG_CATEGORY "withdraw.cpp"


DepositAddressPartMessage GetPartMessageAtPosition(uint32_t position,
                                                   Point address)
{
    uint160 request_hash = depositdata[address]["deposit_request"];
    vector<uint160> part_hashes = depositdata[request_hash]["parts"];
    return msgdata[part_hashes[position]]["deposit_part"];
}

void SendBackupWithdrawalsFromPartMessage(uint160 withdraw_request_hash,
                                          DepositAddressPartMessage part_msg, 
                                          uint32_t position)
{
    log_ << "SendBackupWithdrawalsFromPartMessage: "
         << "request: " << withdraw_request_hash << "  "
         << "part: " << part_msg.GetHash160() << " position: "
         << position << "\n";
    BackupWithdrawalMessage backup_withdraw(withdraw_request_hash,
                                            part_msg.GetHash160(),
                                            position);
    teleportnode.deposit_handler.BroadcastMessage(backup_withdraw);
}

void SendBackupWithdrawalsFromPartMessages(uint160 withdraw_request_hash,
                                           uint32_t position)
{
    log_ << "SendBackupWithdrawalsFromPartMessages: "
         << withdraw_request_hash << "  position: " << position << "\n";
    WithdrawalRequestMessage request;
    request = msgdata[withdraw_request_hash]["withdraw_request"];
    Point address = request.deposit_address;
    DepositAddressPartMessage part_msg = GetPartMessageAtPosition(position,
                                                                  address);
    uint160 part_msg_hash = part_msg.GetHash160();
    vector<Point> relays = part_msg.Relays();
    foreach_(Point relay, relays)
    {
        taskdata[part_msg_hash].Location(relay) = GetTimeMicros();
    }
    for (uint32_t i = 0; i < relays.size(); i++)
    {
        if (keydata[relays[i]].HasProperty("privkey"))
        {
            log_ << "sending backup withdrawal: i = " << i << "\n";
            SendBackupWithdrawalsFromPartMessage(withdraw_request_hash,
                                                 part_msg, i);
        }
    }
}

void SendBackupWithdrawalsFromDisclosure(
    uint160 withdraw_request_hash,
    DepositAddressPartDisclosure disclosure, 
    uint32_t position)
{
    log_ << "SendBackupWithdrawalsFromDisclosure: "
         << disclosure.GetHash160() << " position: " << position << "\n";
    BackupWithdrawalMessage backup_withdraw(withdraw_request_hash,
                                            disclosure.GetHash160(),
                                            position);
    teleportnode.deposit_handler.BroadcastMessage(backup_withdraw);
}

void SendBackupWithdrawalsFromDisclosures(uint160 withdraw_request_hash,
                                          uint32_t position)
{
    log_ << "SendBackupWithdrawalsFromDisclosures: "
         << withdraw_request_hash << " position: " << position << "\n";
    WithdrawalRequestMessage request;
    request = msgdata[withdraw_request_hash]["withdraw_request"];
    Point address = request.deposit_address;
    DepositAddressPartMessage part_msg = GetPartMessageAtPosition(position,
                                                                  address);
    uint160 part_msg_hash = part_msg.GetHash160();
    uint160 disclosure_hash = depositdata[part_msg_hash]["disclosure"];

    uint32_t received = depositdata[withdraw_request_hash][part_msg_hash];

    DepositAddressPartDisclosure disclosure;
    disclosure = msgdata[disclosure_hash]["deposit_disclosure"];
    
    vector<Point> original_relays = part_msg.Relays();
    vector<Point> relays = disclosure.Relays();
    
    for (uint32_t i = 0; i < relays.size(); i++)
    {
        if (received & (1 << i))
            continue;
        DoSuccession(original_relays[i]);
        if (keydata[relays[i]].HasProperty("privkey"))
        {
            log_ << "sending; i = " << i << "\n";
            SendBackupWithdrawalsFromDisclosure(withdraw_request_hash,
                                                disclosure, i);
        }
    }
}

void DoScheduledWithdrawalTimeoutCheck(uint160 withdraw_request_hash)
{
    LOCK(teleportnode.mutex);
    log_ << "DoScheduledWithdrawalTimeoutCheck: "
         << withdraw_request_hash << "\n";
    uint32_t parts_received;
    parts_received = depositdata[withdraw_request_hash]["parts_received"];

    WithdrawalRequestMessage request;
    request = msgdata[withdraw_request_hash]["withdraw_request"];

    bool all_received = true;
    for (uint32_t position = 0; position < SECRETS_PER_DEPOSIT; position++)
    {
        if (!(parts_received & (1 << position)))
        {
            log_ << "sending position " << position << "\n";
            SendBackupWithdrawalsFromPartMessages(withdraw_request_hash,
                                                  position);
            all_received = false;
        }
        else
        {
            log_ << "part " << position << " has been received\n";
        }
    }
    if (!all_received)
        teleportnode.scheduler.Schedule("backup_withdrawal_timeout_check",
                                    withdraw_request_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
}

void MarkTaskAsCompletedByRelay(uint160 task_hash, Point relay)
{
    log_ << "MarkTaskAsCompletedByRelay: " << task_hash 
         << " completed by " << relay << "\n";
    uint64_t task_time = taskdata[task_hash].Location(relay);
    taskdata.RemoveFromLocation(relay, task_time);
}

void DoScheduledWithdrawalRefutationCheck(uint160 complaint_hash)
{
    LOCK(teleportnode.mutex);
    log_ << "DoScheduledWithdrawalRefutationCheck: " << complaint_hash << "\n";
    WithdrawalComplaint complaint;
    complaint = msgdata[complaint_hash]["withdraw_complaint"];
    WithdrawalMessage withdraw = complaint.GetWithdraw();

    if (depositdata[complaint_hash]["refuted"])
    {
        Point relay = withdraw.VerificationKey();
        uint160 request_hash = withdraw.withdraw_request_hash;
        MarkTaskAsCompletedByRelay(request_hash, relay);
        return;
    }
    
    DoSuccession(withdraw.VerificationKey());
    SendBackupWithdrawalsFromPartMessages(withdraw.withdraw_request_hash,
                                          withdraw.position);
}

void DoScheduledBackupWithdrawalTimeoutCheck(uint160 withdraw_request_hash)
{
    LOCK(teleportnode.mutex);
    log_ << "DoScheduledBackupWithdrawalTimeoutCheck: "
         << withdraw_request_hash << "\n";
    uint32_t parts_received;
    parts_received = depositdata[withdraw_request_hash]["parts_received"];

    WithdrawalRequestMessage request;
    request = msgdata[withdraw_request_hash]["withdraw_request"];

    for (uint32_t position = 0; position < SECRETS_PER_DEPOSIT; position++)
    {
        if (!(parts_received & (1 << position)))
        {
            SendBackupWithdrawalsFromDisclosures(withdraw_request_hash,
                                                 position);
        }
    }
}

void DoScheduledBackupWithdrawalRefutationCheck(uint160 complaint_hash)
{
    LOCK(teleportnode.mutex);
    log_ << "DoScheduledBackupWithdrawalRefutationCheck: "
         << complaint_hash << "\n";
    BackupWithdrawalComplaint complaint;
    complaint = msgdata[complaint_hash]["backup_withdraw_complaint"];
    BackupWithdrawalMessage backup_withdraw = complaint.GetBackupWithdraw();

    if (depositdata[complaint_hash]["refuted"])
    {
        Point relay = backup_withdraw.VerificationKey();
        uint160 part_msg_hash = backup_withdraw.GetPartMessage().GetHash160();
        MarkTaskAsCompletedByRelay(part_msg_hash, relay);
        return;
    }

    DoSuccession(backup_withdraw.VerificationKey());
}



/*******************
 * DepositHandler
 */

    void DepositHandler::SendWithdrawalRequestMessage(uint160 address_hash)
    {
    }

    void DepositHandler::HandleWithdrawalRequestMessage(
        WithdrawalRequestMessage request)
    {
        log_ << "HandleWithdrawalRequestMessage: " << request.GetHash160()
             << "\n";
        Point address = request.deposit_address;
        if (depositdata[address]["withdrawn"] ||
            depositdata[address].HasProperty("withdraw_request") ||
            !request.VerifySignature())
        {
            log_ << "signature ok: " << request.VerifySignature() << "\n";
            bool withdrawn = depositdata[address]["withdrawn"];
            log_ << "withdrawn: " << withdrawn << "\n";

            log_ << "invalid\n";
            should_forward = false;
            return;
        }

        log_ << "VerifySignature: " << request.VerifySignature() << "\n";

        uint160 transfer_hash = depositdata[address]["latest_transfer"];

        log_ << "latest_transfer for " << address << " is "
             << transfer_hash << "\n";

        log_ << "request.previous_transfer_hash is "
             << request.previous_transfer_hash << "\n";

        if (request.previous_transfer_hash != transfer_hash)
        {
            log_ << "address does not belong to requestor\n";
            should_forward = false;
            return;
        }
        
        uint160 request_hash = request.GetHash160();
        depositdata[address]["withdraw_request"] = request_hash;

        std::vector<Point> relays = GetRelaysForAddress(address);

        log_ << "got relays: " << relays << "\n";

        for (uint32_t position = 0; position < relays.size(); position++)
        {
            if (keydata[relays[position]].HasProperty("privkey"))
            {
                log_ << "sending withdrawal msg for position " << position
                     << "\n";
                WithdrawalMessage msg(request_hash, position);
                BroadcastMessage(msg);
                log_ << "sent\n";
            }
        }
    }

    void DepositHandler::HandleWithdrawalMessage(
        WithdrawalMessage withdrawal_msg)
    {
        log_ << "HandleWithdrawalMessage: " << withdrawal_msg.GetHash160()
             << "\n";
        if (depositdata[withdrawal_msg.GetDepositAddress()]["withdrawn"] ||
            !withdrawal_msg.VerifySignature())
        {
            log_ << "invalid\n";
            should_forward = false;
            return;
        }
        uint160 msg_hash = withdrawal_msg.GetHash160();

        uint160 withdraw_request_hash = withdrawal_msg.withdraw_request_hash;

        uint32_t received = depositdata[withdraw_request_hash]["parts_received"];
        received |= (1 << withdrawal_msg.position);
        depositdata[withdraw_request_hash]["parts_received"] = received;
        
        if (depositdata[withdrawal_msg.withdraw_request_hash]["is_mine"])
        {
            log_ << "mine - checking and saving secret\n";
            if (!withdrawal_msg.CheckAndSaveSecret())
            {
                log_ << "sending complaint\n";
                WithdrawalComplaint complaint(msg_hash);
                BroadcastMessage(complaint);
            }
            else
            {
                Point address = withdrawal_msg.GetDepositAddress();
                uint160 address_request_hash;
                address_request_hash = depositdata[address]["deposit_request"];
                CheckForAllSecretsOfAddress(address_request_hash);
            }
        }
        teleportnode.scheduler.Schedule("withdrawal_timeout_check",
                                    withdraw_request_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void DepositHandler::HandleWithdrawalComplaint(
        WithdrawalComplaint complaint)
    {
        log_ << "HandleWithdrawalComplaint: " << complaint.GetHash160()
             << "\n";
        Point address = complaint.GetDepositAddress();

        if (!complaint.VerifySignature() || depositdata[address]["withdrawn"])
        {
            should_forward = false;
            return;
        }
        uint160 complaint_hash = complaint.GetHash160();
        uint160 withdraw_request_hash = complaint.GetWithdraw().withdraw_request_hash;
        uint32_t position = complaint.GetWithdraw().position;

        uint32_t received = depositdata[withdraw_request_hash]["parts_received"];
        received &= ~(1 << position);
        depositdata[withdraw_request_hash]["parts_received"] = received;

        depositdata[complaint.withdraw_msg_hash]["complaint"] = complaint_hash;
        teleportnode.scheduler.Schedule("withdrawal_refutation_check",
                                    complaint_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void DepositHandler::HandleWithdrawalRefutation(
        WithdrawalRefutation refutation)
    {
        log_ << "HandleWithdrawalRefutation: " << refutation.GetHash160()
             << "\n";
        if (!refutation.VerifySignature() || !refutation.Validate())
        {
            log_ << "invalid\n";
            should_forward = false;
            return;
        }
        uint160 withdraw_request_hash
            = refutation.GetComplaint().GetWithdraw().withdraw_request_hash;

        uint32_t received = depositdata[withdraw_request_hash]["parts_received"];
        received |= (1 << refutation.GetComplaint().GetWithdraw().position);
        depositdata[withdraw_request_hash]["parts_received"] = received;

        depositdata[refutation.complaint_hash]["refuted"] = true;
    }

    void DepositHandler::HandleBackupWithdrawalMessage(
        BackupWithdrawalMessage backup_withdraw)
    {
        uint160 backup_withdraw_hash = backup_withdraw.GetHash160();
        log_ << "HandleBackupWithdrawalMessage: " << backup_withdraw_hash
             << "\n";
        if (!backup_withdraw.VerifySignature())
        {
            log_ << "invalid\n";
            should_forward = false;
            return;
        }

        Point address = backup_withdraw.GetDepositAddress();
        if (depositdata[address]["withdrawn"])
        {
            log_ << "already withdrawn\n";
            should_forward = false;
            return;
        }
        uint160 withdraw_request_hash = backup_withdraw.withdraw_request_hash;
        uint160 part_msg_hash = backup_withdraw.GetPartMessage().GetHash160();

        uint32_t received = depositdata[withdraw_request_hash][part_msg_hash];
        received |= (1 << backup_withdraw.position_of_secret);
        depositdata[withdraw_request_hash][part_msg_hash] = received;

        if ((!!(received & (1 << 0))) + (!!(received & (1 << 1)))
            + (!!(received & (1 << 2))) + (!!(received & (1 << 3))) >= 3)
        {
            uint32_t received_ 
                = depositdata[withdraw_request_hash]["parts_received"];
            received_ |= (1 << backup_withdraw.GetPartMessage().position);
            depositdata[withdraw_request_hash]["parts_received"] = received_;
        }

        if (depositdata[backup_withdraw.withdraw_request_hash]["is_mine"])
        {
            if (!backup_withdraw.CheckAndSaveSecret())
            {
                log_ << "complaining\n";
                BackupWithdrawalComplaint complaint(backup_withdraw_hash);
                BroadcastMessage(complaint);
            }
            else
            {
                CheckForEnoughSecretsOfAddressPart(backup_withdraw);
            }
        }
    }

    void DepositHandler::CheckForEnoughSecretsOfAddressPart(
        BackupWithdrawalMessage backup_withdraw)
    {
        log_ << "CheckForEnoughSecretsOfAddressPart: "
             << backup_withdraw.GetHash160() << "\n";
        DepositAddressPartMessage part_msg = backup_withdraw.GetPartMessage();
        vector<Point> pubkeys = part_msg.address_part_secret.point_values;
        uint32_t num_secrets = 0;
        foreach_(Point pubkey, pubkeys)
        {
            if (keydata[pubkey].HasProperty("privkey"))
                num_secrets++;
        }
        log_ << "secrets received: " << num_secrets << "\n";
        if (num_secrets >= ADDRESS_PART_SECRETS_NEEDED)
        {
            RecoverSecretPart(part_msg);
            CheckForAllSecretsOfAddress(part_msg.address_request_hash);
        }
    }

    void DepositHandler::RecoverSecretPart(DepositAddressPartMessage part_msg)
    {
        log_ << "RecoverSecretPart: " << part_msg.GetHash160() << "\n";
        CBigNum secret_part;
        vector<Point> pubkeys = part_msg.address_part_secret.point_values;
        vector<CBigNum> secrets;
        foreach_(Point pubkey, pubkeys)
        {
            CBigNum secret = keydata[pubkey]["privkey"];
            secrets.push_back(secret);
        }
        vector<int32_t> cheaters;
        bool cheated;
        generate_private_key_from_N_secret_parts(pubkeys[0].curve,
                                                 secret_part,
                                                 secrets,
                                                 pubkeys,
                                                 SECRETS_PER_ADDRESS_PART,
                                                 cheated,
                                                 cheaters);
        Point pubkey = part_msg.address_part_secret.PubKey();
        if (pubkey != Point(pubkey.curve, secret_part))
        {
            log_ << "Something has gone wrong. Recovered secret "
                 << secret_part << " does not match pubkey " 
                 << pubkey << "\n";
            return;
        }
        keydata[pubkey]["privkey"] = secret_part;
        log_ << "recovered secret for " << pubkey << "\n";
    }

    void DepositHandler::CheckForAllSecretsOfAddress(
        uint160 address_request_hash)
    {
        if (depositdata[address_request_hash]["import_complete"])
            return;

        log_ << "CheckForAllSecretsOfAddress: "
             << address_request_hash << "\n";
        vector<uint160> part_hashes = depositdata[address_request_hash]["parts"];

        CBigNum address_privkey;
        Point address = depositdata[address_request_hash]["address"];

        foreach_(uint160 part_msg_hash, part_hashes)
        {
            DepositAddressPartMessage part_msg;
            part_msg = msgdata[part_msg_hash]["deposit_part"];
            Point pubkey = part_msg.address_part_secret.PubKey();
            if (!keydata[pubkey].HasProperty("privkey"))
            {
                log_ << "missing secret for " << pubkey << "\n";
                return;
            }
            CBigNum secret = keydata[pubkey]["privkey"];
            address_privkey = (address_privkey + secret) % pubkey.Modulus();
        }
        if (Point(address.curve, address_privkey) != address)
        {
            log_ << "Something has gone wrong. Recovered secret "
                 << address_privkey << " does not match address pubkey " 
                 << address << "\n";
            return;
        }
        DepositAddressRequest request;
        request = msgdata[address_request_hash]["deposit_request"];

        log_ << "recovered key for address " << address << "\n";

        keydata[address]["privkey"] = address_privkey;
        if (depositdata[address].HasProperty("offset_point"))
        {
            log_ << address << " has an offset point\n";
            Point offset_point = depositdata[address]["offset_point"];
            log_ << "offset point is " << offset_point << "\n";
            CBigNum offset = keydata[offset_point]["privkey"];
            log_ << "offset is " << offset << "\n";
            address_privkey = (address_privkey + offset) % address.Modulus();
            log_ << "secret address is " << address + offset_point << "\n";
            log_ << "point(privkey) is "
                 << Point(request.curve, address_privkey) << "\n";
            keydata[address + offset_point]["privkey"] = address_privkey;
        }
        
        if (request.currency_code != TCR)
        {
            Currency currency = teleportnode.currencies[request.currency_code];
            log_ << "balance before import: " << currency.Balance() << "\n";
            currency.crypto.ImportPrivateKey(address_privkey);
            log_ << "balance after import: " << currency.Balance() << "\n";
        }
        else
        {
            teleportnode.wallet.ImportPrivateKey(address_privkey);
        }

        RemoveAddress(address, request.currency_code);
        depositdata[address_request_hash]["import_complete"] = true;
    }

    void DepositHandler::HandleBackupWithdrawalComplaint(
        BackupWithdrawalComplaint complaint)
    {
        log_ << "HandleBackupWithdrawalComplaint: "
             << complaint.GetHash160() << "\n";
        Point address = complaint.GetDepositAddress();

        if (!complaint.VerifySignature() || depositdata[address]["withdrawn"])
        {
            should_forward = false;
            return;
        }
        Point relay = complaint.GetBackupWithdraw().VerificationKey();
        if (keydata[relay].HasProperty("privkey"))
        {
            log_ << "refuting\n";
            BackupWithdrawalRefutation refutation(complaint.GetHash160());
            BroadcastMessage(refutation);
        }
        BackupWithdrawalMessage backup_withdraw = complaint.GetBackupWithdraw();

        uint160 withdraw_request_hash = backup_withdraw.withdraw_request_hash;
        uint160 part_msg_hash = backup_withdraw.GetPartMessage().GetHash160();

        uint32_t received = depositdata[withdraw_request_hash][part_msg_hash];
        received &= ~(1 << backup_withdraw.position_of_secret);
        depositdata[withdraw_request_hash][part_msg_hash] = received;

        if ((!!(received & (1 << 0))) + (!!(received & (1 << 1)))
            + (!!(received & (1 << 2))) + (!!(received & (1 << 3))) < 3)
        {
            uint32_t received_ 
                = depositdata[withdraw_request_hash]["parts_received"];
            received_ &= ~(1 << complaint.GetBackupWithdraw().GetPartMessage().position);
            depositdata[withdraw_request_hash]["parts_received"] = received_;
        }

        teleportnode.scheduler.Schedule("backup_withdrawal_refutation_check",
                                    complaint.GetHash160(),
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void DepositHandler::HandleBackupWithdrawalRefutation(
        BackupWithdrawalRefutation refutation)
    {
        log_ << "HandleBackupWithdrawalRefutation: "
             << refutation.GetHash160() << "\n";

        if (!refutation.VerifySignature() || !refutation.Validate())
        {
            log_ << "invalid\n";
            should_forward = false;
            return;
        }
        depositdata[refutation.complaint_hash]["refuted"] = true;

        BackupWithdrawalComplaint complaint = refutation.GetComplaint();
        BackupWithdrawalMessage backup_withdraw = complaint.GetBackupWithdraw();

        uint160 withdraw_request_hash = backup_withdraw.withdraw_request_hash;
        uint160 part_msg_hash = backup_withdraw.GetPartMessage().GetHash160();

        uint32_t received = depositdata[withdraw_request_hash][part_msg_hash];
        received |= (1 << backup_withdraw.position_of_secret);
        depositdata[withdraw_request_hash][part_msg_hash] = received;

        if ((!!(received & (1 << 0))) + (!!(received & (1 << 1)))
            + (!!(received & (1 << 2))) + (!!(received & (1 << 3))) >= 3)
        {
            uint32_t received_ 
                = depositdata[withdraw_request_hash]["parts_received"];
            received_ |= (1 << backup_withdraw.GetPartMessage().position);
            depositdata[withdraw_request_hash]["parts_received"] = received_;
        }
    }

/*
 * DepositHandler
 *******************/
