
#ifndef TELEPORT_WITHDRAWALREQUESTMESSAGE_H
#define TELEPORT_WITHDRAWALREQUESTMESSAGE_H


#include <src/credits/creditsign.h>
#include "DepositTransferMessage.h"

class WithdrawalRequestMessage
{
public:
    Point deposit_address_pubkey;
    uint160 previous_transfer_hash{0};
    Point authorized_key;
    Point recipient_key;
    Signature signature;

    WithdrawalRequestMessage() = default;

    WithdrawalRequestMessage(Point deposit_address_pubkey,
                             Point recipient_key,
                             Data data):
            deposit_address_pubkey(deposit_address_pubkey),
            recipient_key(recipient_key)
    {
        log_ << "WithdrawalRequestMessage: deposit address is " << deposit_address_pubkey << "\n";

        previous_transfer_hash = data.depositdata[deposit_address_pubkey]["latest_transfer"];

        log_ << "previous_transfer_hash is " << previous_transfer_hash << "\n";

        if (previous_transfer_hash == 0)
        {
            authorized_key = data.depositdata[deposit_address_pubkey]["depositor_key"];
        }
        else
        {
            authorized_key = GetPreviousTransfer(data).recipient_pubkey;
        }

        log_ << "authorized_key = " << authorized_key << "\n";
        log_ << "authorized_key has privkey: " << data.keydata[authorized_key].HasProperty("privkey") << "\n";
        log_ << "WithdrawalRequestMessage: recipient_key is " << recipient_key << "\n";
    }

    static string_t Type() { return string_t("withdraw_request"); }

    DEPENDENCIES(previous_transfer_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(deposit_address_pubkey);
        READWRITE(previous_transfer_hash);
        READWRITE(authorized_key);
        READWRITE(recipient_key);
        READWRITE(signature);
    );

    DepositTransferMessage GetPreviousTransfer(Data data)
    {
        return data.GetMessage(previous_transfer_hash);
    }

    Point VerificationKey(Data data)
    {
        log_ << "WithdrawalRequestMessage: VerificationKey()\n";
        uint160 latest_transfer_hash = data.depositdata[deposit_address_pubkey]["latest_transfer"];
        log_ << "latest_transfer_hash is " << latest_transfer_hash << "\n";

        if (latest_transfer_hash != previous_transfer_hash)
        {
            log_ << "latest_transfer_hash does not match "
                 << "previous_transfer_hash " << previous_transfer_hash
                 << "\n";
            return Point(SECP256K1, 0);
        }

        if (previous_transfer_hash != 0)
        {
            DepositTransferMessage previous_transfer = GetPreviousTransfer(data);

            if (previous_transfer.deposit_address_pubkey == deposit_address_pubkey and
                previous_transfer.recipient_pubkey == authorized_key)
            {
                log_ << "ok; returning authorized_key " << authorized_key << "\n";
                return authorized_key;
            }

            return Point(SECP256K1, 0);
        }
        Point key = data.depositdata[deposit_address_pubkey]["depositor_key"];
        log_ << "VerificationKey(): depositor_key for " << deposit_address_pubkey << " is " << key << "\n";
        return key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_WITHDRAWALREQUESTMESSAGE_H
