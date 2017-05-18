#ifndef TELEPORT_SUCCESSIONATTEMPT_H
#define TELEPORT_SUCCESSIONATTEMPT_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include "FourSecretRecoveryMessages.h"
#include "RecoveryFailureAudit.h"



class SuccessionAttempt
{
public:
    bool HasFourRecoveryMessages()
    {
        for (uint32_t i = 0; i < 4; i++)
            if (recovery_message_hashes[i] == 0)
                return false;
        return true;
    }

    FourSecretRecoveryMessages GetFourSecretRecoveryMessages()
    {
        FourSecretRecoveryMessages four_messages;
        four_messages.secret_recovery_messages = recovery_message_hashes;
        return four_messages;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(rejected_recovery_messages);
        READWRITE(recovery_complaint_hashes);
        READWRITE(audits);
        READWRITE(succession_completed_message_hash);
        READWRITE(succession_completed_message_encoded);
        READWRITE(hash_of_message_containing_succession_completed_message);
        READWRITE(succession_completed_audit_message_hash);
        READWRITE(succession_completed_audit_complaints);
        READWRITE(recovery_message_hashes);
    );

    std::vector<uint160> rejected_recovery_messages;
    std::vector<uint160> recovery_complaint_hashes;
    std::map<uint160, RecoveryFailureAudit> audits;
    uint160 succession_completed_message_hash{0};
    uint160 hash_of_message_containing_succession_completed_message{0};
    bool succession_completed_message_encoded{false};
    uint160 succession_completed_audit_message_hash{0};
    std::vector<uint160> succession_completed_audit_complaints;
    std::array<uint160, 4> recovery_message_hashes{{0,0,0,0}};
};


#endif //TELEPORT_SUCCESSIONATTEMPT_H
