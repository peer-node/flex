#ifndef TELEPORT_WITHDRAWALMESSAGE_H
#define TELEPORT_WITHDRAWALMESSAGE_H


#include "WithdrawalRequestMessage.h"
#include "DepositAddressPartMessage.h"

class WithdrawalMessage
{
public:
    uint160 withdraw_request_hash;
    uint32_t position;
    CBigNum secret_xor_shared_secret;
    Signature signature;

    WithdrawalMessage() { }

    WithdrawalMessage(uint160 withdraw_request_hash, uint32_t position, Data data):
            withdraw_request_hash(withdraw_request_hash),
            position(position)
    {
        SetSecret(data);
    }

    void SetSecret(Data data)
    {
        CBigNum secret =  data.keydata[GetAddressPart(data)]["privkey"];
        CBigNum shared_secret = Hash(secret * GetRequest(data).recipient_key);
        secret_xor_shared_secret = secret ^ shared_secret;
    }

    static string_t Type() { return string_t("withdraw"); }

    DEPENDENCIES(withdraw_request_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(withdraw_request_hash);
        READWRITE(position);
        READWRITE(secret_xor_shared_secret);
        READWRITE(signature);
    )

    bool CheckAndSaveSecret(Data data)
    {
        Point recipient_key = GetRequest(data).VerificationKey(data);
        log_ << "CheckAndSaveSecret: recipient_key is " << recipient_key << "\n";
        CBigNum recipient_privkey = data.keydata[recipient_key]["privkey"];
        log_ << "CheckAndSaveSecret: recipient_privkey is "
             << recipient_privkey << "\n";
        Point address_part = GetAddressPart(data);
        log_ << "address_part = " << address_part << "\n";
        CBigNum shared_secret = Hash(address_part * recipient_privkey);
        log_ << "shared_secret is " << shared_secret << "\n";
        log_ << "secret_xor_shared_secret is " << secret_xor_shared_secret << "\n";
        CBigNum secret = secret_xor_shared_secret ^ shared_secret;
        log_ << "secret is " << secret << "\n";
        if (Point(address_part.curve, secret) != address_part)
        {
            log_ << "address_part.curve is " << address_part.curve
                 << " and Point(address_part.curve, secret) is "
                 << Point(address_part.curve, secret) << "\n";
            return false;
        }
        data.keydata[address_part]["privkey"] = secret;
        return true;
    }

    Point GetAddressPart(Data data)
    {
        DepositAddressPartMessage msg = GetPartMessage(data);
        log_ << "GetAddressPart: part message is " << msg.GetHash160() << "\n"
             << "and pubkey is " << msg.address_part_secret.PubKey() << "\n";
        return msg.address_part_secret.PubKey();
    }

    Point GetDepositAddress(Data data)
    {
        return GetRequest(data).deposit_address;
    }

    DepositAddressPartMessage GetPartMessage(Data data)
    {
        uint160 part_msg_hash = GetPartMessageHashes(data)[position];
        log_ << "GetPartMessage: partmessage hashes are: "
             << GetPartMessageHashes(data) << "\n"
             << "part msg hash is: " << part_msg_hash << "\n";

        return data.GetMessage(part_msg_hash);
    }

    std::vector<uint160> GetPartMessageHashes(Data data)
    {
        Point address = GetRequest(data).deposit_address;
        uint160 address_request_hash = data.depositdata[address]["deposit_request"];
        return data.depositdata[address_request_hash]["parts"];
    }

    WithdrawalRequestMessage GetRequest(Data data)
    {
        return data.GetMessage(withdraw_request_hash);
    }

    Point VerificationKey(Data data)
    {
        Point address = GetDepositAddress(data);
        std::vector<Point> relays = GetRelaysForAddress(address);

        if (position >= relays.size())
            return Point(SECP256K1, 0);
        return relays[position];
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_WITHDRAWALMESSAGE_H
