#ifndef TELEPORT_RECOVERYFAILUREAUDIT_H
#define TELEPORT_RECOVERYFAILUREAUDIT_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include "test/teleport_tests/node/relays/messages/FourRecoveryFailureAuditMessages.h"

class RecoveryFailureAudit
{
public:
    std::array<uint160, 4> audit_message_hashes{{0,0,0,0}};
    std::vector<uint160> rejected_audit_messages;
    std::vector<uint160> audit_complaints;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(audit_message_hashes);
        READWRITE(rejected_audit_messages);
        READWRITE(audit_complaints);
    );

    bool FourAuditMessagesHaveBeenReceived()
    {
        for (uint32_t i = 0; i < 4; i++)
            if (audit_message_hashes[i] == 0)
                return false;
        return true;
    }

    FourRecoveryFailureAuditMessages GetFourAuditMessages()
    {
        FourRecoveryFailureAuditMessages four_messages;
        four_messages.audit_messages = audit_message_hashes;
        return four_messages;
    }
};


#endif //TELEPORT_RECOVERYFAILUREAUDIT_H
