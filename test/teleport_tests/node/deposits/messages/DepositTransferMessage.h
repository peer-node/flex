#ifndef TELEPORT_DEPOSITTRANSFERMESSAGE_H
#define TELEPORT_DEPOSITTRANSFERMESSAGE_H


#include <src/credits/creditsign.h>
#include "DepositAddressRequest.h"

class DepositTransferMessage
{
public:
    Point deposit_address_pubkey;
    uint160 previous_transfer_hash;
    Point sender_pubkey;
    Point recipient_pubkey;
    CBigNum offset_xor_shared_secret;
    Signature signature;

    DepositTransferMessage() = default;

    DepositTransferMessage(Point &deposit_address_pubkey, Point &recipient_pubkey, Data data):
            recipient_pubkey(recipient_pubkey),
            deposit_address_pubkey(deposit_address_pubkey)
    {
        Point offset_point = data.depositdata[deposit_address_pubkey]["offset_point"];
        CBigNum offset = data.keydata[offset_point]["privkey"];

        previous_transfer_hash = data.depositdata[deposit_address_pubkey]["latest_transfer"];

        if (previous_transfer_hash == 0)
            sender_pubkey = GetDepositRequest(data).depositor_key;
        else
            sender_pubkey = GetPreviousTransfer(data).recipient_pubkey;

        CBigNum sender_privkey = data.keydata[sender_pubkey]["privkey"];
        CBigNum shared_secret = Hash(sender_privkey * recipient_pubkey);
        offset_xor_shared_secret = offset ^ shared_secret;;
    }

    CBigNum Offset(Data data)
    {
        CBigNum shared_secret = 0;
        if (data.keydata[sender_pubkey].HasProperty("privkey"))
        {
            CBigNum sender_privkey = data.keydata[sender_pubkey]["privkey"];
            shared_secret = Hash(sender_privkey * recipient_pubkey);
        }
        else if (data.keydata[recipient_pubkey].HasProperty("privkey"))
        {
            CBigNum recipient_privkey = data.keydata[recipient_pubkey]["privkey"];
            shared_secret = Hash(sender_pubkey * recipient_privkey);
        }
        return offset_xor_shared_secret ^ shared_secret;
    }

    static string_t Type() { return string_t("transfer"); }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(deposit_address_pubkey);
        READWRITE(previous_transfer_hash);
        READWRITE(sender_pubkey);
        READWRITE(recipient_pubkey);
        READWRITE(offset_xor_shared_secret);
        READWRITE(signature);
    );

    std::vector<uint160> Dependencies()
    {
        std::vector<uint160> dependencies;
        if (previous_transfer_hash != 0)
            dependencies.push_back(previous_transfer_hash);
        return dependencies;
    }

    DepositAddressRequest GetDepositRequest(Data data)
    {
        uint160 request_hash = data.depositdata[deposit_address_pubkey]["deposit_request"];
        return data.GetMessage(request_hash);
    }

    DepositTransferMessage GetPreviousTransfer(Data data)
    {
        return data.GetMessage(previous_transfer_hash);
    }

    Point VerificationKey(Data data)
    {
        if (previous_transfer_hash == 0)
        {
            Point depositor_key = data.depositdata[deposit_address_pubkey]["depositor_key"];
            if (depositor_key == sender_pubkey)
                return sender_pubkey;
            return Point(SECP256K1, 0);
        }

        auto previous_transfer = GetPreviousTransfer(data);
        if (previous_transfer.deposit_address_pubkey == deposit_address_pubkey and
                sender_pubkey == previous_transfer.recipient_pubkey)
            return sender_pubkey;

        return Point(SECP256K1, 0);
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_DEPOSITTRANSFERMESSAGE_H
