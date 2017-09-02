
#ifndef TELEPORT_WITHDRAWALREQUESTMESSAGE_H
#define TELEPORT_WITHDRAWALREQUESTMESSAGE_H


#include <src/credits/creditsign.h>
#include "DepositTransferMessage.h"

class WithdrawalRequestMessage
{
public:
    Point deposit_address;
    uint160 previous_transfer_hash;
    Point authorized_key;
    Point recipient_key;
    Signature signature;

    WithdrawalRequestMessage() { }

    WithdrawalRequestMessage(uint160 deposit_address_hash,
                             Point recipient_key,
                             Data data):
            recipient_key(recipient_key)
    {
        log_ << "deposit address hash is " << deposit_address_hash << "\n";
        deposit_address = data.depositdata[deposit_address_hash]["address"];
        log_ << "deposit address is " << deposit_address << "\n";

        previous_transfer_hash
                = data.depositdata[deposit_address]["latest_transfer"];

        log_ << "WithdrawalRequestMessage: previous_transfer_hash is "
             << previous_transfer_hash << "\n";

        if (previous_transfer_hash == 0)
        {
            authorized_key = data.depositdata[deposit_address]["depositor_key"];
        }
        else
        {
            uint160 authorized_key_hash = GetPreviousTransfer(data).recipient_key_hash;
            authorized_key = data.keydata[authorized_key_hash]["pubkey"];
        }

        log_ << "authorized_key = " << authorized_key << "\n";
        log_ << "authorized_key has privkey: "
             << data.keydata[authorized_key].HasProperty("privkey") << "\n";
        log_ << "WithdrawalRequestMessage: recipient_key is "
             << recipient_key << "\n";
        if (recipient_key.curve == 0 || recipient_key.IsAtInfinity())
        {
            log_ << "invalid recipient_key: using authorized_key\n";
            recipient_key = authorized_key;
        }
        log_ << "WithdrawalRequestMessage: recipient_key is "
             << recipient_key << "\n";
    }

    static string_t Type() { return string_t("withdraw_request"); }

    DEPENDENCIES(previous_transfer_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(deposit_address);
        READWRITE(previous_transfer_hash);
        READWRITE(authorized_key);
        READWRITE(recipient_key);
        READWRITE(signature);
    )

    DepositTransferMessage GetPreviousTransfer(Data data)
    {
        return data.GetMessage(previous_transfer_hash);
    }

    Point VerificationKey(Data data)
    {
        log_ << "WithdrawalRequestMessage: VerificationKey()\n";
        uint160 latest_transfer_hash = data.depositdata[deposit_address]["latest_transfer"];
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
            if (previous_transfer.deposit_address == deposit_address &&
                previous_transfer.recipient_key_hash == KeyHash(authorized_key))
            {
                log_ << "ok; returning authorized_key "
                     << authorized_key << "\n";
                return authorized_key;
            }

            return Point(SECP256K1, 0);
        }
        Point key = data.depositdata[deposit_address]["depositor_key"];
        log_ << "VerificationKey(): depositor_key for " << deposit_address
             << " is " << key << "\n";
        return key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_WITHDRAWALREQUESTMESSAGE_H
