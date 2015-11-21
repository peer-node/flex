// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef FLEX_TRANSFER_MESSAGES
#define FLEX_TRANSFER_MESSAGES

#include "deposit/deposit_messages.h"
#include "deposit/disclosure_messages.h"

#include "log.h"
#define LOG_CATEGORY "transfer_messages.h"

class DepositTransferMessage
{
public:
    Point deposit_address;
    uint160 previous_transfer_hash;
    Point sender_key;
    uint160 recipient_key_hash;
    CBigNum offset_xor_shared_secret;
    Signature signature;

    DepositTransferMessage() { }

    DepositTransferMessage(uint160 deposit_address_hash,
                           uint160 recipient_key_hash):
        recipient_key_hash(recipient_key_hash)
    {
        deposit_address = depositdata[deposit_address_hash]["address"];

        previous_transfer_hash = depositdata[deposit_address]["latest_transfer"];
        if (previous_transfer_hash == 0)
        {
            sender_key = GetDepositRequest().depositor_key;
        }
        else
        {
            uint160 sender_key_hash = GetPreviousTransfer().recipient_key_hash;
            sender_key = keydata[sender_key_hash]["pubkey"];
        }
        log_ << "DepositTransferMessage: recipient_key_hash is "
             << recipient_key_hash << "\n";
    }

    // We need the recipient pubkey to send a secret deposit
    DepositTransferMessage(uint160 deposit_address_hash,
                           Point recipient_key):
        recipient_key_hash(KeyHash(recipient_key))
    {
        deposit_address = depositdata[deposit_address_hash]["address"];
        Point offset_point = depositdata[deposit_address]["offset_point"];
        CBigNum offset = keydata[offset_point]["privkey"];

        previous_transfer_hash = depositdata[deposit_address]["latest_transfer"];
        if (previous_transfer_hash == 0)
        {
            sender_key = GetDepositRequest().depositor_key;
        }
        else
        {
            uint160 sender_key_hash = GetPreviousTransfer().recipient_key_hash;
            sender_key = keydata[sender_key_hash]["pubkey"];
        }

        CBigNum sender_privkey = keydata[sender_key]["privkey"];
        CBigNum shared_secret = Hash(sender_privkey * recipient_key);
        offset_xor_shared_secret = offset ^ shared_secret;

        log_ << "DepositTransferMessage: recipient_key_hash is "
             << recipient_key_hash << "\n";
    }

    static string_t Type() { return string_t("transfer"); }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(deposit_address);
        READWRITE(previous_transfer_hash);
        READWRITE(sender_key);
        READWRITE(recipient_key_hash);
        READWRITE(offset_xor_shared_secret);
        READWRITE(signature);
    )

    std::vector<uint160> Dependencies()
    {
        std::vector<uint160> dependencies;
        if (previous_transfer_hash != 0)
            dependencies.push_back(previous_transfer_hash);
        return dependencies;
    }

    DepositAddressRequest GetDepositRequest()
    {
        uint160 request_hash = depositdata[deposit_address]["deposit_request"];
        return msgdata[request_hash]["deposit_request"];
    }

    DepositTransferMessage GetPreviousTransfer()
    {
        return msgdata[previous_transfer_hash]["transfer"];
    }

    Point VerificationKey()
    {
        if (previous_transfer_hash != 0)
        {
            DepositTransferMessage previous_transfer = GetPreviousTransfer();
            uint160 sender_key_hash = KeyHash(sender_key);

            if (previous_transfer.deposit_address == deposit_address &&
                sender_key_hash == previous_transfer.recipient_key_hash)
                return sender_key;
            return Point(SECP256K1, 0);
        }
        Point depositor_key = depositdata[deposit_address]["depositor_key"];
        if (depositor_key == sender_key)
            return sender_key;
        return Point(SECP256K1, 0);
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

typedef std::pair<uint160, uint160> uint160pair;

class TransferAcknowledgement
{
public:
    uint160 transfer_hash;
    std::vector<std::pair<uint160, uint160> > disqualifications;
    Signature signature;

    TransferAcknowledgement() { }

    TransferAcknowledgement(uint160 transfer_hash):
        transfer_hash(transfer_hash)
    {
        Point deposit_address = GetTransfer().deposit_address;
        disqualifications = depositdata[deposit_address]["disqualifications"];
    }

    static string_t Type() { return string_t("transfer_ack"); }

    std::vector<uint160> Dependencies()
    {
        std::vector<uint160> dependencies;
        dependencies.push_back(transfer_hash);

        foreach_(uint160pair disqualification, disqualifications)
        {
            dependencies.push_back(disqualification.first);
            dependencies.push_back(disqualification.second);
        }
        
        return dependencies;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(transfer_hash);
        READWRITE(disqualifications);
        READWRITE(signature);
    )

    bool CheckDisqualifications()
    {
        std::vector<Point> relays = Relays();
        for (uint32_t i = 0; i < disqualifications.size(); i++)
        {
            uint160 ack_hash1 = disqualifications[i].first;
            uint160 ack_hash2 = disqualifications[i].second;

            TransferAcknowledgement ack1, ack2;
            ack1 = msgdata[ack_hash1]["transfer_ack"];
            ack2 = msgdata[ack_hash2]["transfer_ack"];

            if (ack1.VerificationKey() != relays[i] ||
                ack2.VerificationKey() != relays[i])
                return false;

            DepositTransferMessage transfer1 = ack1.GetTransfer();
            DepositTransferMessage transfer2 = ack2.GetTransfer();
            
            if (transfer1.previous_transfer_hash != 
                transfer2.previous_transfer_hash)
                return false;

            if (transfer1.deposit_address != transfer2.deposit_address)
                return false;

            if (transfer1.recipient_key_hash == transfer2.recipient_key_hash)
                return false;
        }
        return true;
    }

    std::vector<Point> Relays()
    {
        return GetRelaysForAddress(GetDepositAddress());
    }

    Point GetDepositAddress()
    {
        return GetTransfer().deposit_address;
    }

    DepositTransferMessage GetTransfer()
    {
        return msgdata[transfer_hash]["transfer"];
    }

    bool Validate()
    {
        log_ << "TransferAcknowledgement::Validate: "
             << "CheckDisqualifications: " << CheckDisqualifications() << "\n"
             << "VerifySignature: " << VerifySignature() << "\n";
        return CheckDisqualifications() && VerifySignature();
    }

    Point VerificationKey()
    {
        uint32_t num_disqualified_relays = disqualifications.size();
        std::vector<Point> relays = Relays();
        log_ << "VerificationKey: relays are " << relays << "\n";
        Point key = relays[num_disqualified_relays];
        log_ << "VerificationKey is " << key << "\n";
        return key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif