#include "flexnode/flexnode.h"

#include "log.h"
#define LOG_CATEGORY "deposits.cpp"


std::vector<Point> GetRelaysForAddressRequest(uint160 request_hash,
                                              uint160 encoding_credit_hash)
{
    log_ << "GetRelaysForAddressRequest: encoding_credit_hash is "
         << encoding_credit_hash << " and request hash is "
         << request_hash << "\n";
    if (encoding_credit_hash == 0)
        encoding_credit_hash = depositdata[request_hash]["encoding_credit_hash"];
    else
        depositdata[request_hash]["encoding_credit_hash"] = encoding_credit_hash;
    MinedCredit mined_credit = creditdata[encoding_credit_hash]["mined_credit"];
    log_ << "GetRelaysForAddressRequest: encoding mined is " << mined_credit;
    uint160 relay_chooser = mined_credit.GetHash160() ^ request_hash;
    RelayState state = GetRelayState(encoding_credit_hash);
    log_ << "Relay state retrieved is: " << state;
    return state.ChooseRelays(relay_chooser, SECRETS_PER_DEPOSIT);
}

std::vector<Point> GetRelaysForAddress(Point address)
{
    uint160 request_hash = depositdata[address]["deposit_request"];
    uint160 encoding_credit_hash;
    encoding_credit_hash = depositdata[request_hash]["encoding_credit_hash"];
    return GetRelaysForAddressRequest(request_hash, encoding_credit_hash);
}


void DoScheduledAddressRequestTimeoutCheck(uint160 request_hash)
{
    LOCK(flexnode.mutex);
    uint32_t parts_received = depositdata[request_hash]["parts_received"];
    Point address = depositdata[request_hash]["address"];

    vector<Point> relays = GetRelaysForAddress(address);

    bool all_received = true;
    for(uint32_t position = 0; position < SECRETS_PER_DEPOSIT; position++)
    {
        Point relay = relays[position];
        if (!(parts_received & (1 << position)))
        {
            if (flexnode.RelayState().relays[relay] > INHERITANCE_START)
            {
                log_ << "relay " << position << " has not responded to "
                     << "address request " << request_hash
                     << "; doing succession\n";
                DoSuccession(relay);
            }
            all_received = false;
        }
    }
    if (!all_received)
        flexnode.scheduler.Schedule("request_timeout_check",
                                    request_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
}

void DoScheduledAddressPartRefutationCheck(uint160 complaint_hash)
{
    LOCK(flexnode.mutex);
    if (depositdata[complaint_hash]["refuted"])
        return;
    DepositAddressPartComplaint complaint;
    complaint = msgdata[complaint_hash]["deposit_part_complaint"];
    DepositAddressPartMessage part_msg;
    part_msg = msgdata[complaint.part_msg_hash]["deposit_part"];
    flexnode.deposit_handler.CancelRequest(part_msg.address_request_hash);

    Point bad_relay = part_msg.VerificationKey();
    flexnode.RelayState().AddDisqualification(bad_relay, complaint_hash);
    DoSuccession(bad_relay);
}


void InitializeDepositScheduledTasks()
{
    flexnode.scheduler.AddTask(
        ScheduledTask("request_timeout_check",
                      DoScheduledAddressRequestTimeoutCheck));

    flexnode.scheduler.AddTask(
        ScheduledTask("part_refutation_check",
                      DoScheduledAddressPartRefutationCheck));

    flexnode.scheduler.AddTask(
        ScheduledTask("disclosure_timeout_check",
                      DoScheduledDisclosureTimeoutCheck));

    flexnode.scheduler.AddTask(
        ScheduledTask("disclosure_refutation_check",
                      DoScheduledDisclosureRefutationCheck));

    flexnode.scheduler.AddTask(
        ScheduledTask("post_disclosure_check",
                      DoScheduledPostDisclosureCheck));

    flexnode.scheduler.AddTask(
        ScheduledTask("confirm_transfer",
                      DoScheduledConfirmTransferCheck));

    flexnode.scheduler.AddTask(
        ScheduledTask("withdrawal_timeout_check",
                      DoScheduledWithdrawalTimeoutCheck));

    flexnode.scheduler.AddTask(
        ScheduledTask("withdrawal_refutation_check",
                      DoScheduledWithdrawalRefutationCheck));

    flexnode.scheduler.AddTask(
        ScheduledTask("backup_withdrawal_timeout_check",
                      DoScheduledBackupWithdrawalTimeoutCheck));

    flexnode.scheduler.AddTask(
        ScheduledTask("backup_withdrawal_refutation_check",
                      DoScheduledBackupWithdrawalRefutationCheck));
}


/*******************
 * DepositHandler
 */

    void DepositHandler::SendDepositAddressRequest(vch_t currency,
                                                   uint8_t curve,
                                                   bool secret=false)
    {
        DepositAddressRequest request(curve, currency);
        request.DoWork();
        request.Sign();
        uint160 request_hash = request.GetHash160();
        depositdata[request_hash]["is_mine"] = true;
        if (secret)
        {
            log_ << "SendDepositAddressRequest: secret\n";
            CBigNum secret_offset = RandomPrivateKey(curve);
            log_ << "secret offset is " << secret_offset << "\n";
            Point offset_point(curve, secret_offset);
            log_ << "offset point is " << offset_point << "\n";
            depositdata[request_hash]["offset_point"] = offset_point;
            log_ << "recorded offset point of " << offset_point
                 << "for " << request_hash << "\n";
            keydata[offset_point]["privkey"] = secret_offset;
        }
        log_ << "sending address request " << request_hash << "\n";
        BroadcastMessage(request);
    }

    void DepositHandler::HandleDepositAddressRequest(
        DepositAddressRequest request)
    {
        uint160 request_hash = request.GetHash160();
        log_ << "HandleDepositAddressRequest: " << request_hash << "\n";
        if (!flexnode.downloader.finished_downloading)
        {
            log_ << "not finished downloading\n";
            return;
        }
        
        log_ << "handling address request " << request_hash << "\n";
        if (depositdata[request_hash]["processed"]||
            !request.VerifySignature() ||
            !request.CheckWork())
        {
            log_ << "failed validation\n";
            should_forward = false;
            return;
        }

        flexnode.pit.HandleRelayMessage(request_hash);

        depositdata[request_hash]["processed"] = true;
    }

    void DepositHandler::HandleDepositAddressPartMessage(
        DepositAddressPartMessage msg)
    {
        log_ << "HandleDepositAddressPartMessage: " << msg.GetHash160();
        uint160 part_msg_hash = msg.GetHash160();
        if (depositdata[part_msg_hash]["processed"] || 
            !msg.VerifySignature() ||
            !msg.address_part_secret.Validate())
        {
            should_forward = false;
            return;
        }
        
        uint160 request_hash = msg.address_request_hash;

        uint32_t parts_received = depositdata[request_hash]["parts_received"];

        if (parts_received & (1 << msg.position))
        {
            CancelRequest(request_hash);
            DoSuccession(msg.VerificationKey());
            return;
        }

        if (CheckAndSaveSharesFromAddressPart(msg, msg.position))
        {
            parts_received |= (1 << msg.position);
            depositdata[request_hash]["parts_received"] = parts_received;

            std::vector<uint160> part_hashes;
            part_hashes = depositdata[msg.address_request_hash]["parts"];

            part_hashes.push_back(part_msg_hash);
            depositdata[msg.address_request_hash]["parts"] = part_hashes;

            if (part_hashes.size() == SECRETS_PER_DEPOSIT)
                HandleDepositAddressParts(msg.address_request_hash, 
                                          part_hashes);
        }
        if (!initdata[part_msg_hash]["from_history"])
            flexnode.scheduler.Schedule("part_complaint_check",
                                        part_msg_hash,
                                        GetTimeMicros() + COMPLAINT_WAIT_TIME);

        depositdata[part_msg_hash]["processed"] = true;
    }

    bool DepositHandler::CheckAndSaveSharesFromAddressPart(
        DepositAddressPartMessage msg,
        uint32_t position)
    {
        std::vector<uint32_t> bad_secrets;
        uint160 msg_hash = msg.GetHash160();
        log_ << "CheckAndSaveSharesFromAddressPart: "  << msg_hash
             << " at position " << position << "\n";
        if (!msg.address_part_secret.ValidateAndStoreMySecrets(bad_secrets))
        {
            foreach_(uint32_t position, bad_secrets)
            {
                DepositAddressPartComplaint complaint(msg_hash, position);
                BroadcastMessage(complaint);
            }
        }
        return bad_secrets.size() == 0;
    }

    void DepositHandler::HandleDepositAddressParts(
        uint160 request_hash,
        std::vector<uint160> part_hashes)
    {
        log_ << "HandleDepositAddressParts: " << request_hash << "\n";
        DepositAddressRequest request;
        DepositAddressPartMessage part_msg;
        request = msgdata[request_hash]["deposit_request"];

        Point address(request.curve, 0);
        foreach_(uint160 part_msg_hash, part_hashes)
        {
             part_msg = msgdata[part_msg_hash]["deposit_part"];
             address += part_msg.PubKey();
        }
        depositdata[request_hash]["address"] = address;
        log_ << "address for " << request_hash << " is " << address << "\n";

        depositdata[address]["deposit_request"] = request_hash;
        log_ << "request_hash for " << address << " is " 
             << request_hash << "\n";
        
        depositdata[address]["depositor_key"] = request.depositor_key;
        log_ << "depositor_key for " << address << " is "
             << request.depositor_key << "\n";

        if (depositdata[request_hash]["is_mine"])
        {
            AddAddress(address, request.currency_code);
            uint160 address_hash = Hash160(address.getvch());
            log_ << "HandleDepositAddressParts: recording "
                 << address_hash << " has address " << address << "\n";
            depositdata[address_hash]["address"] = address;
            depositdata[KeyHash(address)]["address"] = address;
            depositdata[FullKeyHash(address)]["address"] = address;
            keydata[address_hash]["pubkey"] = address;

            if (depositdata[request_hash].HasProperty("offset_point"))
            {
                Point offset_point = depositdata[request_hash]["offset_point"];
                log_ << "recording offset_point of " << offset_point
                     << " for " << address << "\n";
                depositdata[address]["offset_point"] = offset_point;
                uint160 secret_address_hash = KeyHash(address + offset_point);
                if (request.currency_code == FLX)
                {
                    walletdata[secret_address_hash]["watched"] = true;
                }
            }
            else
                log_ << "no offset point for " << request_hash << "\n";
        }
    }

    void DepositHandler::HandleDepositAddressPartComplaint(
        DepositAddressPartComplaint complaint)
    {
        log_ << "HandleDepositAddressPartComplaint: " 
             << complaint.GetHash160() << "\n";
        uint160 complaint_hash = complaint.GetHash160();
        uint160 part_msg_hash = complaint.part_msg_hash;
        if (!complaint.VerifySignature())
        {
            should_forward = false;
            return;
        }

        if (depositdata[part_msg_hash].HasProperty("disclosure"))
        {
            log_ << "disclosure for " << part_msg_hash 
                 << " already received. too late to complain\n";
            should_forward = false;
            return;
        }

        vector<uint160> complaint_hashes;
        complaint_hashes = depositdata[part_msg_hash]["complaints"];

        complaint_hashes.push_back(complaint_hash);
        depositdata[part_msg_hash]["complaints"] = complaint_hashes;

        DepositAddressPartMessage part_msg;
        part_msg = msgdata[complaint.part_msg_hash]["deposit_part"];
        Point relay = part_msg.VerificationKey();

        if (keydata[relay].HasProperty("privkey"))
        {
            DepositAddressPartRefutation refutation(complaint_hash);
            BroadcastMessage(refutation);
        }
        flexnode.scheduler.Schedule("part_refutation_check",
                                    complaint_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void DepositHandler::HandleDepositAddressPartRefutation(
        DepositAddressPartRefutation refutation)
    {
        uint160 refutation_hash = refutation.GetHash160();
        log_ << "HandleDepositAddressPartRefutation: "
             << refutation_hash << "\n";
        if (refutation.Validate() && refutation.VerifySignature())
        {
            uint160 complaint_hash = refutation.complaint_hash;
            depositdata[complaint_hash]["refuted"] = true;
            DepositAddressPartComplaint complaint;
            complaint = depositdata[complaint_hash]["deposit_part_complaint"];
            Point bad_relay = complaint.VerificationKey();
            flexnode.RelayState().AddDisqualification(bad_relay, 
                                                      refutation_hash);
            DoSuccession(bad_relay);
        }
        else
        {
            should_forward = false;
            return;
        }
    }

    void DepositHandler::HandleEncodedRequests(MinedCreditMessage& msg)
    {
        if (!flexnode.downloader.finished_downloading)
            return;
        uint160 credit_hash = msg.mined_credit.GetHash160();
        log_ << "HandleEncodedRequests: " << credit_hash << "\n";
        msg.hash_list.RecoverFullHashes();

        foreach_(uint160 hash, msg.hash_list.full_hashes)
        {
            log_ << "Encoded hash: " << hash << "\n";
            string_t type = msgdata[hash]["type"];
            log_ << "type is " << type << "\n";

            if (type == "deposit_request")
            {
                log_ << "handling encoded request: " << hash << "\n";
                HandleEncodedRequest(hash, credit_hash);
            }
        }
    }

    void DepositHandler::HandleEncodedRequest(uint160 request_hash,
                                              uint160 encoding_credit_hash)
    {

        log_ << "HandleEncodedRequest: " << request_hash 
             << "encoded in " << encoding_credit_hash << "\n";
        uint64_t now = GetTimeMicros();
        bool inherited = depositdata[request_hash]["inherited"];

        if (depositdata[request_hash].HasProperty("encoding_credit_hash")
            && !inherited)
            return;

        if (inherited)
        {
            bool done = depositdata[request_hash]["inherited_and_processed"];
            if (done)
                return;

            depositdata[request_hash]["inherited_and_processed"] = true;
        }
        depositdata[request_hash]["encoding_credit_hash"] = encoding_credit_hash;

        if (initdata[encoding_credit_hash]["from_history"] ||
            initdata[encoding_credit_hash]["from_datamessage"])
            return;

        vector<Point> relays
            = GetRelaysForAddressRequest(request_hash, encoding_credit_hash);

        uint32_t parts_received = depositdata[request_hash]["parts_received"];

        for (uint32_t position = 0; position < relays.size(); position++)
        {
            Point relay = relays[position];
            taskdata[request_hash].Location(relay) = now;

            if (parts_received & (1 << position))
                continue;

            if (keydata[relay].HasProperty("privkey"))
            {
                DepositAddressPartMessage part_msg(request_hash,
                                                   encoding_credit_hash,
                                                   position);
                BroadcastMessage(part_msg);
            }
        }
        if (depositdata[request_hash]["scheduled"])
            return;

        flexnode.scheduler.Schedule("request_timeout_check",
                                    request_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
        depositdata[request_hash]["scheduled"] = true;
    }

    void DepositHandler::HandlePostEncodedRequests(MinedCreditMessage& msg)
    {
        if (!flexnode.downloader.finished_downloading)
            return;
        uint160 credit_hash = msg.mined_credit.GetHash160();
        log_ << "HandlePostEncodedRequests: " << credit_hash << "\n";

        uint160 previous_hash = msg.mined_credit.previous_mined_credit_hash;
        MinedCreditMessage prev_msg = creditdata[previous_hash]["msg"];

        prev_msg.hash_list.RecoverFullHashes();
        foreach_(uint160 hash, prev_msg.hash_list.full_hashes)
        {
            string_t type = msgdata[hash]["type"];
            if (type == "deposit_request")
            {
                log_ << "post-encoded request: " << hash << "\n";
                HandlePostEncodedRequest(hash, credit_hash);
            }
        }
    }

    void DepositHandler::AddBatchToTip(MinedCreditMessage& msg)
    {
        HandleEncodedRequests(msg);
        HandlePostEncodedRequests(msg);
    }

    void DepositHandler::RemoveBatchFromTip()
    {

    }

    void DepositHandler::InitiateAtCalend(uint160 calend_hash)
    {

    }

    void DepositHandler::CancelRequest(uint160 request_hash)
    {
        log_ << "cancelling request_hash\n";
        depositdata[request_hash]["cancelled"] = true;
    }

/*
 * DepositHandler
 *******************/
