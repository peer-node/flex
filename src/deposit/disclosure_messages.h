// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef FLEX_DISCLOSURE_MESSAGES
#define FLEX_DISCLOSURE_MESSAGES

#include "deposit/deposit_messages.h"

#include "log.h"
#define LOG_CATEGORY "disclosure_messages.h"

class DepositAddressPartDisclosure
{
public:
    uint160 address_part_message_hash;
    AddressPartSecretDisclosure disclosure;
    Signature signature;

    DepositAddressPartDisclosure() { }

    DepositAddressPartDisclosure(uint160 post_encoding_credit_hash,
                                 uint160 address_part_message_hash):
        address_part_message_hash(address_part_message_hash)
    {
        log_ << "DepositAddressPartDisclosure: address_part_message_hash = "
             << address_part_message_hash << "\n";

        AddressPartSecret address_part_secret;
        address_part_secret = GetPartMessage().address_part_secret;
        uint160 relay_chooser = address_part_message_hash ^
                                    post_encoding_credit_hash;
        disclosure = AddressPartSecretDisclosure(post_encoding_credit_hash,
                                                 address_part_secret,
                                                 relay_chooser);
    }

    static string_t Type() { return string_t("deposit_disclosure"); }

    DEPENDENCIES(address_part_message_hash);

    std::vector<Point> Relays()
    {
        return disclosure.Relays();
    }

    DepositAddressPartMessage GetPartMessage()
    {
        return msgdata[address_part_message_hash]["deposit_part"];
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(address_part_message_hash);
        READWRITE(disclosure);
        READWRITE(signature);
    )

    Point VerificationKey()
    {
        return GetPartMessage().VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class DepositDisclosureComplaint
{
public:
    uint160 disclosure_hash;
    uint32_t number_of_secret;
    CBigNum secret;
    Signature signature;

    DepositDisclosureComplaint() { }

    DepositDisclosureComplaint(uint160 disclosure_hash,
                               uint32_t number_of_secret,
                               CBigNum secret):
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

    DepositAddressPartDisclosure GetDisclosure()
    {
        return msgdata[disclosure_hash]["deposit_disclosure"];
    }

    bool Validate()
    {
        if (number_of_secret > GetDisclosure().Relays().size())
            return false;
        if (secret != 0)
        {
            DepositAddressPartDisclosure disclosure = GetDisclosure();
            uint160 part_msg_hash = disclosure.address_part_message_hash;
            DepositAddressPartMessage part_msg;
            part_msg = msgdata[part_msg_hash]["deposit_part"];
            return disclosure.disclosure.ValidatePurportedlyBadSecret(
                                            part_msg.address_part_secret,
                                            number_of_secret,
                                            secret);
        }
        return true;
    }

    Point VerificationKey()
    {
        return GetDisclosure().Relays()[number_of_secret];
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class DepositDisclosureRefutation
{
public:
    uint160 complaint_hash;
    CBigNum secret;
    Signature signature;

    DepositDisclosureRefutation() { }

    DepositDisclosureRefutation(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    {
        secret = GetPartMessage().GetSecret(GetComplaint().number_of_secret);
    }

    static string_t Type() 
    {
        return string_t("deposit_disclosure_refutation");
    }

    DEPENDENCIES(complaint_hash);

    bool Validate()
    {
        uint32_t number_of_secret = GetComplaint().number_of_secret;
        DepositAddressPartMessage part_msg = GetPartMessage();
        Point point = part_msg.GetPoint(number_of_secret);
        if (point != Point(point.curve, secret))
            return false;

        CBigNum shared_secret = Hash(secret * GetComplaint().VerificationKey());
        std::vector<CBigNum> secrets_xor_shared_secrets
            = GetDisclosure().disclosure.secrets_xor_shared_secrets;
        if (secrets_xor_shared_secrets[number_of_secret] !=
            (shared_secret ^ secret))
            return false;
        return part_msg.CheckSecretAtPosition(secret, number_of_secret);
    }

    DepositAddressPartDisclosure GetDisclosure()
    {
        return GetComplaint().GetDisclosure();
    }

    DepositAddressPartMessage GetPartMessage()
    {
        return GetDisclosure().GetPartMessage();
    }

    DepositDisclosureComplaint GetComplaint()
    {
        return msgdata[complaint_hash]["deposit_disclosure_complaint"];
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(secret);
        READWRITE(signature);
    )

    Point VerificationKey()
    {
        return GetComplaint().GetDisclosure().VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

#endif