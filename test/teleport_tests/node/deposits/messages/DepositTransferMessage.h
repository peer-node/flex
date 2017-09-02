#ifndef TELEPORT_DEPOSITTRANSFERMESSAGE_H
#define TELEPORT_DEPOSITTRANSFERMESSAGE_H


#include <src/credits/creditsign.h>
#include "DepositAddressRequest.h"

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

    DepositAddressRequest GetDepositRequest(Data data)
    {
        uint160 request_hash = data.depositdata[deposit_address]["deposit_request"];
        return data.GetMessage(request_hash);
    }

    DepositTransferMessage GetPreviousTransfer(Data data)
    {
        return data.GetMessage(previous_transfer_hash);
    }

    Point VerificationKey(Data data)
    {
        if (previous_transfer_hash != 0)
        {
            DepositTransferMessage previous_transfer = GetPreviousTransfer(data);
            uint160 sender_key_hash = KeyHash(sender_key);

            if (previous_transfer.deposit_address == deposit_address &&
                sender_key_hash == previous_transfer.recipient_key_hash)
                return sender_key;
            return Point(SECP256K1, 0);
        }
        Point depositor_key = data.depositdata[deposit_address]["depositor_key"];
        if (depositor_key == sender_key)
            return sender_key;
        return Point(SECP256K1, 0);
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_DEPOSITTRANSFERMESSAGE_H
