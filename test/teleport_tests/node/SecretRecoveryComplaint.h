#ifndef TELEPORT_SECRETRECOVERYCOMPLAINT_H
#define TELEPORT_SECRETRECOVERYCOMPLAINT_H


#include <src/crypto/signature.h>
#include "Data.h"
#include "Relay.h"
#include "SecretRecoveryMessage.h"

class SecretRecoveryComplaint
{
public:
    uint160 secret_recovery_message_hash{0};
    std::vector<uint32_t> positions_of_bad_encrypted_secrets;
    Signature signature;

    static std::string Type() { return "secret_recovery_complaint"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(secret_recovery_message_hash);
        READWRITE(positions_of_bad_encrypted_secrets);
        READWRITE(signature);
    );

    JSON(secret_recovery_message_hash, positions_of_bad_encrypted_secrets, signature);

    DEPENDENCIES(secret_recovery_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    Relay *GetComplainer(Data data);

    Relay *GetSecretSender(Data data);

    SecretRecoveryMessage GetSecretRecoveryMessage(Data data);
};


#endif //TELEPORT_SECRETRECOVERYCOMPLAINT_H
