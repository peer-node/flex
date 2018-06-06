#ifndef TELEPORT_FOURRECOVERYFAILUREAUDITMESSAGES_H
#define TELEPORT_FOURRECOVERYFAILUREAUDITMESSAGES_H

#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include "define.h"
#include <array>

class FourRecoveryFailureAuditMessages
{
public:
    static std::string Type() { return "four_recovery_failure_audit_messages"; }

    FourRecoveryFailureAuditMessages() { }

    FourRecoveryFailureAuditMessages(uint160 hash1, uint160 hash2, uint160 hash3, uint160 hash4)
    {
        audit_messages[0] = hash1;
        audit_messages[1] = hash2;
        audit_messages[2] = hash3;
        audit_messages[3] = hash4;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(audit_messages);
    );

    HASH160();

    std::array<uint160, 4> audit_messages{{0,0,0,0}};
};


#endif //TELEPORT_FOURRECOVERYFAILUREAUDITMESSAGES_H
