// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "flexnode/flexnode.h"

#include "log.h"
#define LOG_CATEGORY "disclosure.cpp"


void DoScheduledDisclosureRefutationCheck(uint160 complaint_hash)
{
    log_ << "DoScheduledDisclosureRefutationCheck: " << complaint_hash << "\n";
    if (depositdata[complaint_hash]["refuted"])
        return;
    DepositDisclosureComplaint complaint;
    complaint = msgdata[complaint_hash]["deposit_disclosure_complaint"];
    DepositAddressPartDisclosure disclosure;
    disclosure = msgdata[complaint.disclosure_hash]["deposit_disclosure"];

    DepositAddressPartMessage part_msg;
    part_msg = msgdata[disclosure.address_part_message_hash]["deposit_part"];

    uint160 request_hash = part_msg.address_request_hash;
    flexnode.deposit_handler.CancelRequest(request_hash);

    Point bad_relay = disclosure.VerificationKey();
    flexnode.RelayState().AddDisqualification(bad_relay, complaint_hash);
    DoSuccession(bad_relay);
}

void DoScheduledPostDisclosureCheck(uint160 request_hash)
{
    log_ << "DoScheduledPostDisclosureCheck: " << request_hash << "\n";
    if (depositdata[request_hash]["cancelled"])
    {
        log_ << request_hash << " has been cancelled\n";
        return;
    }
    Point address = depositdata[request_hash]["address"];
    log_ << "deposit address created: " << address << "\n";
    uint160 address_hash = KeyHash(address);
    depositdata[address_hash]["address"] = address;
    log_ << "address for " << address_hash << " is " << address << "\n";
    log_ << "keyhash_ is " << FullKeyHash(address) << "\n";
    DepositAddressRequest request = msgdata[request_hash]["deposit_request"];
    string address_string;
    if (request.currency_code != FLX)
    {
        Currency currency = flexnode.currencies[request.currency_code];
        address_string = currency.crypto.PubKeyToAddress(address);
    }
    else
    {
        address_string = FlexAddressFromPubKey(address);
    }
    log_ << "final address is " << address_string << "\n";
    CBitcoinAddress address_(address_string);
    CKeyID keyid;
    address_.GetKeyID(keyid);
    vch_t keydata_(BEGIN(keyid), END(keyid));
    log_ << "keydata is " << keydata_ << "\n";
    uint160 hash(keydata_);
    log_ << "hash is " << hash << "\n";
    depositdata[hash]["address"] = address;
    CKeyID keyID(hash);
    log_ << "btc address is " << CBitcoinAddress(keyID).ToString() << "\n";
    depositdata[address_hash]["confirmed"] = true;
    depositdata[FullKeyHash(address)]["confirmed"] = true;
}

void DoScheduledDisclosureTimeoutCheck(uint160 part_msg_hash)
{
    log_ << "DoScheduledDisclosureTimeoutCheck: " << part_msg_hash << "\n";
    if (!depositdata[part_msg_hash].HasProperty("disclosure"))
    {
        log_ << part_msg_hash << " has not been disclosed" 
             << "; doing succession\n";
        DepositAddressPartMessage part_msg;
        part_msg = msgdata[part_msg_hash]["deposit_part"];

        vector<Point> relays = part_msg.Relays();
        DoSuccession(relays[part_msg.position]);
        
        depositdata[part_msg.address_request_hash]["cancelled"] = true;
    }
}


/*******************
 * DepositHandler
 */

    void DepositHandler::HandlePostEncodedRequest(
        uint160 request_hash,
        uint160 post_encoding_credit_hash)
    {
        log_ << "HandlePostEncodedRequest: " << request_hash << "\n";
        DepositAddressPartMessage part_msg;
        std::vector<uint160> part_hashes = depositdata[request_hash]["parts"];
        foreach_(uint160 part_msg_hash, part_hashes)
        {
            part_msg = msgdata[part_msg_hash]["deposit_part"];
            Point relay = part_msg.VerificationKey();
            log_ << "part " << part_msg_hash << " was sent by "
                 << relay << "\n";
            if (keydata[relay].HasProperty("privkey"))
            {
                log_ << "have key for " << relay << "; sending disclosure\n";
                DepositAddressPartDisclosure disclosure(
                    post_encoding_credit_hash, part_msg_hash);
                BroadcastMessage(disclosure);
                log_ << "sent disclosure\n";
            }
            flexnode.scheduler.Schedule("disclosure_timeout_check",
                                        part_msg_hash,
                                        GetTimeMicros() + COMPLAINT_WAIT_TIME);
        }
    }

    void DepositHandler::HandleDepositAddressPartDisclosure(
        DepositAddressPartDisclosure disclosure)
    {
        uint160 disclosure_hash = disclosure.GetHash160();
        log_ << "HandleDepositAddressPartDisclosure: " 
             << disclosure.GetHash160() << "\n";
        if (!disclosure.VerifySignature() || !disclosure.disclosure.Validate())
        {
            should_forward = false;
            return;
        }
        uint160 part_msg_hash = disclosure.address_part_message_hash;
        if (depositdata[part_msg_hash].HasProperty("disclosure"))
        {
            DepositAddressPartDisclosure original_disclosure;
            uint160 original_disclosure_hash;
            original_disclosure_hash = depositdata[part_msg_hash]["disclosure"];
            original_disclosure
                = msgdata[original_disclosure_hash]["deposit_disclosure"];
            if (original_disclosure.GetHash160() != disclosure_hash)
            {
                DepositAddressPartMessage part_msg;
                part_msg = disclosure.GetPartMessage();
                uint160 request_hash = part_msg.address_request_hash;
                log_ << "cancelling request: " << request_hash << "\n";
                CancelRequest(request_hash);
            }
            
            return;
        }
        CheckAndSaveDisclosureSecrets(disclosure);
        depositdata[part_msg_hash]["disclosure"] = disclosure_hash;
        CheckForFinalDisclosure(disclosure);
    }

    void DepositHandler::CheckForFinalDisclosure(
        DepositAddressPartDisclosure disclosure)
    {
        uint160 disclosure_hash = disclosure.GetHash160();
        log_ << "CheckForFinalDisclosure: " << disclosure_hash << "\n";
        uint160 part_msg_hash = disclosure.address_part_message_hash;
        DepositAddressPartMessage part_msg;
        part_msg = msgdata[part_msg_hash]["deposit_part"];
        uint160 request_hash = part_msg.address_request_hash;

        std::vector<uint160> disclosure_hashes;
        disclosure_hashes = depositdata[request_hash]["disclosures"];
        if (!VectorContainsEntry(disclosure_hashes, disclosure_hash))
        {
            disclosure_hashes.push_back(disclosure_hash);
            depositdata[request_hash]["disclosures"] = disclosure_hashes;
        }
        if (disclosure_hashes.size() == SECRETS_PER_DEPOSIT)
        {
            flexnode.scheduler.Schedule("post_disclosure_check",
                                        request_hash,
                                        GetTimeMicros() + COMPLAINT_WAIT_TIME);
        }
    }

    void DepositHandler::CheckAndSaveDisclosureSecrets(
        DepositAddressPartDisclosure disclosure)
    {
        log_ << "CheckAndSaveDisclosureSecrets: " << disclosure.GetHash160()
             << "\n";
        uint160 part_msg_hash = disclosure.address_part_message_hash;
        DepositAddressPartMessage part_msg;
        log_ << "looking up part_msg with hash: " << part_msg_hash;

        part_msg = msgdata[part_msg_hash]["deposit_part"];

        log_ << "part_msg.address_part_secret.credit_hash is "
             << part_msg.address_part_secret.credit_hash << "\n";

        std::vector<uint32_t> bad_disclosure_secrets;
        BigNumsInPositions bad_original_secrets;

        if (!disclosure.disclosure.ValidateAndStoreMySecrets(
                                        part_msg.address_part_secret,
                                        bad_disclosure_secrets,
                                        bad_original_secrets))
        {
            foreach_(uint32_t position, bad_disclosure_secrets)
            {
                log_ << "complaining about secret " << position << "\n";
                DepositDisclosureComplaint complaint(disclosure.GetHash160(),
                                                     position, 0);
                BroadcastMessage(complaint);
            }
            foreach_(const BigNumsInPositions::value_type item,
                     bad_original_secrets)
            {
                uint32_t position = item.first;
                CBigNum secret = item.second;
                log_ << "complaining about original secret "
                     << position << "\n";
                DepositDisclosureComplaint complaint(disclosure.GetHash160(),
                                                     position, secret);
                BroadcastMessage(complaint);
            }
        }
    }

    void DepositHandler::HandleDepositDisclosureComplaint(
        DepositDisclosureComplaint complaint)
    {
        uint160 complaint_hash = complaint.GetHash160();
        log_ << "HandleDepositDisclosureComplaint: "
             << complaint_hash << "\n";
        
        if (!complaint.Validate() || !complaint.VerifySignature())
        {
            log_ << "invalid complaint\n";
            should_forward = false;
            return;
        }

        DepositAddressPartDisclosure disclosure;
        disclosure = msgdata[complaint.disclosure_hash]["deposit_disclosure"];
        Point relay = disclosure.VerificationKey();

        if (keydata[relay].HasProperty("privkey"))
        {
            log_ << "sending refutation\n";
            DepositDisclosureRefutation refutation(complaint_hash);
            BroadcastMessage(refutation);
        }

        flexnode.scheduler.Schedule("disclosure_refutation_check",
                                    complaint_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void DepositHandler::HandleDepositDisclosureRefutation(
        DepositDisclosureRefutation refutation)
    {
        log_ << "HandleDepositDisclosureRefutation: "
             << refutation.GetHash160() << "\n";
        if (!refutation.Validate() || !refutation.VerifySignature())
        {
            log_ << "invalid refutation\n";
            should_forward = false;
            return;
        }
        depositdata[refutation.complaint_hash]["refuted"] = true;
        depositdata[refutation.complaint_hash]["refutation"] = refutation;
    }


/*
 * DepositHandler
 *******************/
