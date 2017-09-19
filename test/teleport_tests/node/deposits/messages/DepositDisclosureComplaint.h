#ifndef TELEPORT_DEPOSITDISCLOSURECOMPLAINT_H
#define TELEPORT_DEPOSITDISCLOSURECOMPLAINT_H


#include "DepositAddressPartDisclosure.h"

class DepositDisclosureComplaint
{
public:
    uint160 disclosure_hash;
    uint32_t number_of_secret;
    CBigNum secret;
    Signature signature;

    DepositDisclosureComplaint() { }

    DepositDisclosureComplaint(uint160 disclosure_hash, uint32_t number_of_secret, CBigNum secret):
            disclosure_hash(disclosure_hash),
            number_of_secret(number_of_secret),
            secret(secret)
    { }

    static string_t Type() { return string_t("deposit_disclosure_complaint"); }

    DEPENDENCIES(disclosure_hash);

    IMPLEMENT_SERIALIZE
    (
    READWRITE(disclosure_hash);
    READWRITE(number_of_secret);
    READWRITE(secret);
    READWRITE(signature);
    )

    DepositAddressPartDisclosure GetDisclosure(Data data)
    {
        return data.GetMessage(disclosure_hash);
    }

    bool Validate(Data data)
    {
//        if (number_of_secret > GetDisclosure(data).Relays(data).size())
//            return false;
//        if (secret != 0)
//        {
//            DepositAddressPartDisclosure disclosure = GetDisclosure(data);
//            uint160 part_msg_hash = disclosure.address_part_message_hash;
//            DepositAddressPartMessage part_msg = data.GetMessage(part_msg_hash);
//            return disclosure.disclosure.ValidatePurportedlyBadSecret(
//                    part_msg.address_part_secret,
//                    number_of_secret,
//                    secret);
//        }
        return true;
    }

    Point VerificationKey(Data data)
    {
        //return GetDisclosure(data).Relays(data)[number_of_secret];
        return Point(0);
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_DEPOSITDISCLOSURECOMPLAINT_H
