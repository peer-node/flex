#ifndef TELEPORT_GOODBYEREFUTATION_H
#define TELEPORT_GOODBYEREFUTATION_H

#include <src/crypto/uint256.h>
#include <src/crypto/signature.h>
#include "Data.h"
#include "GoodbyeComplaint.h"


class GoodbyeRefutation
{
public:
    uint160 complaint_hash;
    Signature signature;

    static std::string Type() { return "goodbye_refutation"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(signature);
    );

    JSON(complaint_hash, signature);

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    GoodbyeComplaint GetComplaint(Data data);
};


#endif //TELEPORT_GOODBYEREFUTATION_H
