
#ifndef TELEPORT_DEPOSITADDRESSPARTDISCLOSURE_H
#define TELEPORT_DEPOSITADDRESSPARTDISCLOSURE_H


#include "DepositAddressPartMessage.h"
#include "AddressPartSecretDisclosure.h"


class DepositAddressPartDisclosure
{
public:
    uint160 address_part_message_hash{0};
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

    std::array<std::array<uint64_t, SECRETS_PER_ADDRESS_PART>, AUDITORS_PER_SECRET> Auditors()
    {
        return disclosure.auditor_numbers;
    }

    uint32_t Position(Data data)
    {
        return GetPartMessage(data).position;
    }

    DepositAddressPartMessage GetPartMessage(Data data)
    {
        return data.GetMessage(address_part_message_hash);
    }

    uint160 GetEncodedRequestIdentifier(Data data)
    {
        auto part_message = GetPartMessage(data);
        return part_message.EncodedRequestIdentifier();
    }

    bool CheckDisclosedSecretsAreCorrect(std::vector<uint32_t> &positions_of_bad_secrets, Data data)
    {
        DepositAddressPartMessage part_message = data.GetMessage(address_part_message_hash);
        // todo
        return true;
    }

    bool CheckSecretsFromAddressPartMessageAreCorrect(std::vector<uint32_t> &positions_of_bad_secrets, Data data)
    {
        DepositAddressPartMessage part_message = data.GetMessage(address_part_message_hash);
        // todo
        return true;
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
