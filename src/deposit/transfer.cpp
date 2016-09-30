// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "teleportnode/teleportnode.h"

#include "log.h"
#define LOG_CATEGORY "transfer.cpp"

#define NETWORK_DIAMETER (4 * 1000 * 1000) // 4 seconds
#define TRANSFER_CONFIRM_TIME (2 * NETWORK_DIAMETER)

Point GetRespondingRelay(Point deposit_address)
{
    vector<Point> relays = GetRelaysForAddress(deposit_address);

    vector<pair<uint160, uint160> > disqualifications = depositdata[deposit_address]["disqualifications"];

    if (disqualifications.size() >= relays.size())
        return Point(SECP256K1, 0);

    return relays[disqualifications.size()];
}

void AddAddress(Point address, vch_t currency)
{
    vector<Point> my_addresses = depositdata[currency]["addresses"];

    if (!VectorContainsEntry(my_addresses, address))
        my_addresses.push_back(address);
    
    depositdata[currency]["addresses"] = my_addresses;

    if (currency == TCR)
        walletdata[KeyHash(address)]["watched"] = true;
}

void RemoveAddress(Point address, vch_t currency)
{
    vector<Point> my_addresses = depositdata[currency]["addresses"];

    if (VectorContainsEntry(my_addresses, address))
        EraseEntryFromVector(address, my_addresses);
    
    depositdata[currency]["addresses"] = my_addresses;
}

void AddAndRemoveMyAddresses(uint160 transfer_hash)
{
    log_ << "AddAndRemoveMyAddresses: " << transfer_hash << "\n";
    DepositTransferMessage transfer = msgdata[transfer_hash]["transfer"];
    DepositAddressRequest request = transfer.GetDepositRequest();
    vch_t currency = request.currency_code;
    log_ << "currency is " << currency << "\n";
    
    Point address = transfer.deposit_address;
    log_ << "address is " << address << "\n";

    Point sender_key = transfer.VerificationKey();
    log_ << "sender_key is " << sender_key << "\n";

    if (keydata[sender_key].HasProperty("privkey"))
    {
        log_ << "Sent by me - removing\n";
        RemoveAddress(address, currency);
    }

    log_ << "recipient_key_hash is " << transfer.recipient_key_hash << "\n";

    log_ << "have pubkey: "
         << keydata[transfer.recipient_key_hash].HasProperty("pubkey") << "\n";

    if (keydata[transfer.recipient_key_hash].HasProperty("pubkey"))
    {
        AddAddress(address, currency);
        uint160 address_hash = KeyHash(address);
        depositdata[address_hash]["address"] = address;
    }
}

void DoScheduledConfirmTransferCheck(uint160 ack_hash)
{
    log_ << "DoScheduledConfirmTransferCheck: " << ack_hash << "\n";
    if (depositdata[ack_hash]["conflicted"])
    {
        log_ << "Conflict detected. Not confirming\n";
        return;
    }
    TransferAcknowledgement ack = msgdata[ack_hash]["transfer_ack"];
    Point address = ack.GetDepositAddress();
    uint160 latest_transfer_hash = depositdata[address]["latest_transfer"];
    if (latest_transfer_hash == ack.transfer_hash)
    {
        depositdata[ack.transfer_hash]["confirmed"] = true;
        log_ << "transfer of " << address << " confirmed\n";
        AddAndRemoveMyAddresses(ack.transfer_hash);
    }
}

bool TooLateForOverride(TransferAcknowledgement acknowledgement)
{
    Point address = acknowledgement.GetDepositAddress();
    uint160 latest_transfer_hash = depositdata[address]["latest_transfer"];
    uint160 conflicting_transfer_hash = acknowledgement.transfer_hash;
    uint64_t first_transfer_time = msgdata[latest_transfer_hash]["received_at"];
    uint64_t conflicting_transfer_time
        = msgdata[conflicting_transfer_hash]["received_at"];
    uint64_t override_time = GetTimeMicros();

    if (conflicting_transfer_time < first_transfer_time)
        return false;

    uint64_t time_between_transfers = conflicting_transfer_time
                                        - first_transfer_time;

    if (time_between_transfers > TRANSFER_CONFIRM_TIME)
        return true;

    if (time_between_transfers > NETWORK_DIAMETER &&
        override_time - conflicting_transfer_time > NETWORK_DIAMETER)
        return true;

    return false;
}

bool TooLateForConflict(TransferAcknowledgement acknowledgement)
{
    Point address = acknowledgement.GetDepositAddress();
    uint160 latest_transfer_hash = depositdata[address]["latest_transfer"];
    uint64_t first_transfer_time = msgdata[latest_transfer_hash]["received_at"];
    
    uint64_t conflict_time = GetTimeMicros();

    return conflict_time - first_transfer_time > TRANSFER_CONFIRM_TIME;
}

void CheckForSecretDepositAddress(DepositTransferMessage transfer)
{
    if (!keydata[transfer.recipient_key_hash].HasProperty("pubkey"))
        return;
    Point recipient_key = keydata[transfer.recipient_key_hash]["pubkey"];
    CBigNum privkey = keydata[recipient_key]["privkey"];
    if (privkey == 0)
        return;
    CBigNum shared_secret = Hash(privkey * transfer.sender_key);
    CBigNum offset = transfer.offset_xor_shared_secret ^ shared_secret;
    Point offset_point(transfer.deposit_address.curve, offset);
    keydata[transfer.deposit_address]["offset_point"] = offset_point;
    keydata[offset_point]["privkey"] = offset;
}


/*******************
 * DepositHandler
 */

    void DepositHandler::HandleDepositTransferMessage(
        DepositTransferMessage transfer)
    {
        should_forward = false; // transfers are pushed directly for speed

        uint160 transfer_hash = transfer.GetHash160();
        log_ << "HandleDepositTransferMessage: " << transfer_hash << "\n";
        Point address = transfer.deposit_address;

        if (!depositdata[address].HasProperty("deposit_request"))
        {
            RequestedDataMessage data_request;
            vector<Point> addresses;
            addresses.push_back(address);
            vector<uint160> empty;
            
            uint160 request_hash
                = teleportnode.downloader.datahandler.SendDataRequest(
                    empty, empty, empty, addresses, GetPeer(transfer_hash));
            initdata[request_hash]["queued_transfer"] = transfer_hash;
            return;
        }

        if (depositdata[address]["withdrawn"] ||
            !transfer.VerifySignature())
        {
            log_ << "invalid\n";
            return;
        }

        uint160 latest_transfer_hash = depositdata[address]["latest_transfer"];
        if (latest_transfer_hash != transfer.previous_transfer_hash)
        {
            log_ << "that address belongs to somebody else\n";
            return;    
        }

        if (depositdata[transfer_hash]["sent"] && 
            !depositdata[transfer_hash]["inherited"])
            return;

        if (depositdata[transfer_hash]["inherited"] && 
            depositdata[transfer_hash]["inherited_and_handled"])
            return;

        if (!depositdata[transfer_hash]["inherited"])
            PushDirectlyToPeers(transfer);

        if (transfer.offset_xor_shared_secret != 0)
            CheckForSecretDepositAddress(transfer);

        Point relay = GetRespondingRelay(transfer.deposit_address);
        log_ << "responding relay is " << relay << "\n";
        if (keydata[relay].HasProperty("privkey"))
        {
            log_ << "sending acknowledgement";
            TransferAcknowledgement acknowledgement(transfer_hash);
            PushDirectlyToPeers(acknowledgement);
        }
        if (depositdata[transfer_hash]["inherited"])
            depositdata[transfer_hash]["inherited_and_handled"] = true;
    }

    void DepositHandler::HandleConflictingAcknowledgement(
        TransferAcknowledgement ack,
        uint160 prev_transfer_hash)
    {
        uint160 ack_hash = depositdata[prev_transfer_hash]["acknowledgement"];
        log_ << "HandleConflictingAcknowledgement: " << ack_hash << "\n";
        TransferAcknowledgement existing_ack;
        existing_ack = msgdata[ack_hash]["acknowledgement"];

        DepositTransferMessage existing_transfer = existing_ack.GetTransfer();
        DepositTransferMessage new_transfer = ack.GetTransfer();

        if (existing_transfer.previous_transfer_hash
            == new_transfer.previous_transfer_hash &&
            existing_transfer.recipient_key_hash 
            != new_transfer.recipient_key_hash)
        {
            if (existing_ack.VerificationKey() == ack.VerificationKey())
            {
                log_ << "two acks of transfers from same deposit to "
                     << "different recipients.\nkey: "
                     << ack.VerificationKey() << " is disqualified\n";
                AddDisqualification(existing_ack, ack);
                SendAnotherAckIfAppropriate(existing_ack);
            }
        }
    }

    void DepositHandler::SendAnotherAckIfAppropriate(
        TransferAcknowledgement existing_ack)
    {
        log_ << "SendAnotherAckIfAppropriate: "
             << existing_ack.GetHash160() << "\n";
        
        Point address = existing_ack.GetTransfer().deposit_address;
        Point relay = GetRespondingRelay(address);
        
        if (keydata[relay].HasProperty("privkey"))
        {
            log_ << "sending acknowledgement\n";
            TransferAcknowledgement acknowledgement(existing_ack.transfer_hash);
            PushDirectlyToPeers(acknowledgement);
        }
    }

    void DepositHandler::AddDisqualification(TransferAcknowledgement ack1,
                                             TransferAcknowledgement ack2)
    {
        std::pair<uint160, uint160> disqualification;

        Point address = ack1.GetTransfer().deposit_address;
        uint160 ack1_hash = ack1.GetHash160();
        uint160 ack2_hash = ack2.GetHash160();

        log_ << "AddDisqualification: " << ack1_hash << " and "
             << ack2_hash << "\n";
        disqualification = std::make_pair(ack1_hash, ack2_hash);        
        std::vector<std::pair<uint160, uint160> > disqualifications = depositdata[address]["disqualifications"];

        Point relay = ack1.VerificationKey();
        depositdata[address][relay] = string_t("disqualified");

        if (!VectorContainsEntry(disqualifications, disqualification))
            disqualifications.push_back(disqualification);
        depositdata[address]["disqualifications"] = disqualifications;
        DoSuccession(ack1.VerificationKey());
    }

    void DepositHandler::HandleTransferAcknowledgement(
        TransferAcknowledgement acknowledgement)
    {
        should_forward = false; // pushed directly to peers for speed
        uint160 ack_hash = acknowledgement.GetHash160();

        log_ << "HandleTransferAcknowledgement: " << ack_hash << "\n";

        if (!msgdata[acknowledgement.transfer_hash].HasProperty("transfer"))
        {
            log_ << "no transfer " << acknowledgement.transfer_hash
                 << " found\n";
            return;
        }

        log_ << "transfer hash: " << acknowledgement.transfer_hash
             << " does have a message\n";

        DepositTransferMessage transfer = acknowledgement.GetTransfer();
        Point address = transfer.deposit_address;
        log_ << "deposit address is " << address << "\n";

        if (depositdata[ack_hash]["queued"] && !depositdata[ack_hash]["ready"])
        {
            log_ << "this acknowledgement will be processed when it's ready\n";
            return;
        }

        if (!depositdata[address].HasProperty("deposit_request"))
        {
            log_ << "no deposit request found - requesting data\n";
            RequestedDataMessage data_request;
            vector<Point> addresses;
            addresses.push_back(address);
            vector<uint160> empty;
            
            uint160 request_hash
                = teleportnode.downloader.datahandler.SendDataRequest(
                    empty, empty, empty, addresses, GetPeer(ack_hash));
            initdata[request_hash]["queued_ack"] = ack_hash;
            depositdata[ack_hash]["queued"] = true;
            return;
        }
        else
        {
            log_ << "deposit address request found; proceeding\n";
        }

        if (depositdata[ack_hash]["processed"])
        {
            log_ << "already handled\n";
            return;
        }
        if (!acknowledgement.Validate())
        {
            log_ << "invalid\n";
            return;
        }

        uint160 last_transfer_hash = depositdata[address]["latest_transfer"];

        uint160 prev_transfer_hash = transfer.previous_transfer_hash;

        bool conflict = false, override = false;

        if (prev_transfer_hash != last_transfer_hash)
        {
            if (depositdata[last_transfer_hash]["confirmed"])            
            {
                log_ << "transfer " << last_transfer_hash << " has been "
                     << "confirmed; can't transfer from "
                     << prev_transfer_hash << "\n";
                return;
            }

            DepositTransferMessage prev_transfer;
            prev_transfer = msgdata[prev_transfer_hash]["transfer"];

            if (prev_transfer.previous_transfer_hash == prev_transfer_hash)
            {
                TransferAcknowledgement existing_ack;
                uint160 existing_ack_hash
                    = depositdata[prev_transfer_hash]["acknowledgement"];
                existing_ack = msgdata[existing_ack_hash]["transfer_ack"];

                if (existing_ack.disqualifications.size()
                        == acknowledgement.disqualifications.size())
                {
                    log_ << "conflicting acknowledgement\n";
                    HandleConflictingAcknowledgement(acknowledgement,
                                                     prev_transfer_hash);
                    if (!TooLateForConflict(acknowledgement))
                        depositdata[existing_ack_hash]["conflicted"] = true;
                    conflict = true;
                }
                else if (acknowledgement.disqualifications.size()
                         > existing_ack.disqualifications.size())
                {
                    log_ << "this acknowledgement overrides "
                         << existing_ack_hash << "\n";
                    override = true;
                }
                else  // ack has fewer disqualifications than existing ack
                {
                    return;
                }
            }
        }

        std::vector<std::pair<uint160, uint160> > known_disqualifications
            = depositdata[address]["disqualifications"];
        
        if (known_disqualifications.size() > 
            acknowledgement.disqualifications.size())
        {
            log_ << "acknowledgement author is disqualified\n";
            return;
        }

        if (!initdata[ack_hash]["from_history"])
            PushDirectlyToPeers(acknowledgement);

        if (override && TooLateForOverride(acknowledgement))
        {
            log_ << "too late for override\n";
            return;
        }

        if (conflict)
            return;

        uint160 transfer_hash = acknowledgement.transfer_hash;
        depositdata[transfer_hash]["acknowledgement"] = ack_hash;
        depositdata[address]["latest_transfer"] = transfer_hash;
        log_ << "HandleTransferAcknowledgement: transfer_hash for "
             << address << " is now " << transfer_hash << "\n";

        if (!initdata[ack_hash]["from_history"] ||
            depositdata[ack_hash]["ready"])
        {
            log_ << "transfer ok; scheduling confirmation\n";
            teleportnode.scheduler.Schedule("confirm_transfer",
                                        ack_hash,
                                        GetTimeMicros() + TRANSFER_CONFIRM_TIME);
        }
        depositdata[ack_hash]["processed"] = true;

        if (override)
            depositdata[transfer_hash]["conflicted"] = false;
    }

/*
 * DepositHandler
 *******************/
