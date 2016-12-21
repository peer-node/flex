#ifndef TELEPORT_SECRETRECOVERYREFUTATION_H
#define TELEPORT_SECRETRECOVERYREFUTATION_H


#include <src/crypto/uint256.h>
#include <src/crypto/signature.h>
#include "SecretRecoveryComplaint.h"
#include "Data.h"


class SecretRecoveryRefutation
{
public:
    uint160 complaint_hash;
    Signature signature;

    static std::string Type() { return "secret_recovery_refutation"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(signature);
    );

    JSON(complaint_hash, signature);

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    SecretRecoveryComplaint GetComplaint(Data data);
};


#endif //TELEPORT_SECRETRECOVERYREFUTATION_H
