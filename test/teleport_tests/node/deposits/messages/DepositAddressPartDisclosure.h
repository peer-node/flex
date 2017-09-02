
#ifndef TELEPORT_DEPOSITADDRESSPARTDISCLOSURE_H
#define TELEPORT_DEPOSITADDRESSPARTDISCLOSURE_H


#include "DepositAddressPartMessage.h"
#include "AddressPartSecretDisclosure.h"


class DepositAddressPartDisclosure
{
public:
    uint160 address_part_message_hash;
    AddressPartSecretDisclosure disclosure;
    Signature signature;

    DepositAddressPartDisclosure() { }

    DepositAddressPartDisclosure(uint160 post_encoding_credit_hash, uint160 address_part_message_hash, Data data):
            address_part_message_hash(address_part_message_hash)
    {
        log_ << "DepositAddressPartDisclosure: address_part_message_hash = " << address_part_message_hash << "\n";

        AddressPartSecret address_part_secret;
        address_part_secret = GetPartMessage(data).address_part_secret;
        uint160 relay_chooser = address_part_message_hash xor post_encoding_credit_hash;
        disclosure = AddressPartSecretDisclosure(post_encoding_credit_hash, address_part_secret, relay_chooser, data);
    }

    static string_t Type() { return string_t("deposit_disclosure"); }

    DEPENDENCIES(address_part_message_hash);

    std::vector<Point> Relays()
    {
        return disclosure.Relays();
    }

    DepositAddressPartMessage GetPartMessage(Data data)
    {
        return data.GetMessage(address_part_message_hash);
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(address_part_message_hash);
        READWRITE(disclosure);
        READWRITE(signature);
    )

    Point VerificationKey(Data data)
    {
        return GetPartMessage(data).VerificationKey(data);
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_DEPOSITADDRESSPARTDISCLOSURE_H
