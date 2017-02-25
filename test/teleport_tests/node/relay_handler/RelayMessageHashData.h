#ifndef TELEPORT_RELAYMESSAGEHASHDATA_H
#define TELEPORT_RELAYMESSAGEHASHDATA_H


#include <src/crypto/uint256.h>
#include <test/teleport_tests/teleport_data/MemoryObject.h>


class RelayMessageHashData
{
public:
    uint160 join_message_hash{0};
    uint160 mined_credit_message_hash{0};
    uint160 key_distribution_message_hash{0};
    uint160 goodbye_message_hash{0};
    uint160 obituary_hash{0};
    std::vector<uint160> secret_recovery_failure_message_hashes;
    std::vector<uint160> key_distribution_complaint_hashes;
    std::vector<uint160> secret_recovery_message_hashes;
    std::vector<uint160> secret_recovery_complaint_hashes;
    std::vector<uint160> recovery_failure_audit_message_hashes;
    std::vector<uint160> goodbye_complaint_hashes;
    std::vector<uint160> succession_completed_message_hashes;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(join_message_hash);
        READWRITE(mined_credit_message_hash);
        READWRITE(key_distribution_message_hash);
        READWRITE(goodbye_message_hash);
        READWRITE(obituary_hash);
        READWRITE(secret_recovery_failure_message_hashes);
        READWRITE(key_distribution_complaint_hashes);
        READWRITE(secret_recovery_message_hashes);
        READWRITE(secret_recovery_complaint_hashes);
        READWRITE(recovery_failure_audit_message_hashes);
        READWRITE(goodbye_complaint_hashes);
        READWRITE(succession_completed_message_hashes);
    );

    JSON(join_message_hash, mined_credit_message_hash, key_distribution_message_hash, goodbye_message_hash,
         obituary_hash, secret_recovery_failure_message_hashes, key_distribution_complaint_hashes,
         secret_recovery_message_hashes, secret_recovery_complaint_hashes, recovery_failure_audit_message_hashes,
         goodbye_complaint_hashes, succession_completed_message_hashes);

    HASH160();
};


#endif //TELEPORT_RELAYMESSAGEHASHDATA_H
