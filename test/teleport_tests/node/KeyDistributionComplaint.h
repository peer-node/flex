#ifndef TELEPORT_KEYDISTRIBUTIONCOMPLAINT_H
#define TELEPORT_KEYDISTRIBUTIONCOMPLAINT_H

#include <src/crypto/uint256.h>
#include <src/crypto/signature.h>
#include "Data.h"
#include "KeyDistributionMessage.h"

#define KEY_DISTRIBUTION_COMPLAINT_KEY_QUARTERS 0
#define KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS 1
#define KEY_DISTRIBUTION_COMPLAINT_SECOND_KEY_SIXTEENTHS 2


class KeyDistributionComplaint
{
public:
    KeyDistributionComplaint() { }

    KeyDistributionComplaint(KeyDistributionMessage key_distribution_message,
                             uint64_t set_of_secrets,
                             uint64_t position_of_secret);

    uint160 key_distribution_message_hash{0};
    uint64_t set_of_secrets{0};
    uint64_t position_of_secret{0};
    Signature signature;

    static std::string Type() { return "key_distribution_complaint"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(key_distribution_message_hash);
        READWRITE(set_of_secrets);
        READWRITE(position_of_secret);
        READWRITE(signature);
    );

    JSON(key_distribution_message_hash, set_of_secrets, position_of_secret, signature);

    DEPENDENCIES(key_distribution_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    Relay *GetComplainer(Data data);

    Relay *GetSecretSender(Data data);
};


#endif //TELEPORT_KEYDISTRIBUTIONCOMPLAINT_H
