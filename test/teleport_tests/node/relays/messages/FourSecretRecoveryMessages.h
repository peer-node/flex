#ifndef TELEPORT_FOURSECRETRECOVERYMESSAGES_H
#define TELEPORT_FOURSECRETRECOVERYMESSAGES_H

#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include "define.h"

class FourSecretRecoveryMessages
{
public:
    static std::string Type() { return "four_secret_recovery_messages"; }

    FourSecretRecoveryMessages() { }

    FourSecretRecoveryMessages(uint160 hash1, uint160 hash2, uint160 hash3, uint160 hash4)
    {
        secret_recovery_messages[0] = hash1;
        secret_recovery_messages[1] = hash2;
        secret_recovery_messages[2] = hash3;
        secret_recovery_messages[3] = hash4;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(secret_recovery_messages);
    );

    JSON(secret_recovery_messages);

    HASH160();

    std::array<uint160, 4> secret_recovery_messages{{0,0,0,0}};
};


#endif //TELEPORT_FOURSECRETRECOVERYMESSAGES_H
