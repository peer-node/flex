#ifndef TELEPORT_DEPOSITADDRESSPARTCOMPLAINT_H
#define TELEPORT_DEPOSITADDRESSPARTCOMPLAINT_H


#include "DepositAddressPartMessage.h"

class DepositAddressPartComplaint
{
public:
    uint160 part_msg_hash{0};
    uint32_t number_of_secret{0};
    Signature signature;

    DepositAddressPartComplaint() = default;

    DepositAddressPartComplaint(uint160 part_msg_hash,
                                uint32_t number_of_secret):
            part_msg_hash(part_msg_hash),
            number_of_secret(number_of_secret)
    { }

    static string_t Type() { return string_t("deposit_part_complaint"); }

    DEPENDENCIES(part_msg_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(part_msg_hash);
        READWRITE(number_of_secret);
        READWRITE(signature);
    )

    DepositAddressPartMessage GetPartMessage(Data data)
    {
        return data.GetMessage(part_msg_hash);
    }

    Point VerificationKey(Data data)
    {
        return GetPartMessage(data).VerificationKey(data);
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_DEPOSITADDRESSPARTCOMPLAINT_H
