#ifndef TELEPORT_KEYDISTRIBUTIONREFUTATION_H
#define TELEPORT_KEYDISTRIBUTIONREFUTATION_H


#include <src/crypto/uint256.h>
#include <src/crypto/signature.h>
#include "Data.h"
#include "Relay.h"
#include "KeyDistributionComplaint.h"

class KeyDistributionRefutation
{
public:
    uint160 complaint_hash;
    Signature signature;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(signature);
    )

    JSON(complaint_hash, signature);

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    Relay *GetComplainer(Data data);

    KeyDistributionComplaint GetComplaint(Data data);

    Relay *GetSecretSender(Data data);
};


#endif //TELEPORT_KEYDISTRIBUTIONREFUTATION_H
