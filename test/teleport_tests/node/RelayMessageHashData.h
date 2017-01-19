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
    std::vector<uint160> secret_recovery_message_hashes;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(join_message_hash);
        READWRITE(mined_credit_message_hash);
        READWRITE(key_distribution_message_hash);
        READWRITE(goodbye_message_hash);
        READWRITE(obituary_hash);
        READWRITE(secret_recovery_message_hashes);
    );

    JSON(join_message_hash, mined_credit_message_hash, key_distribution_message_hash, goodbye_message_hash,
         obituary_hash, secret_recovery_message_hashes);

    HASH160();

    void Store(MemoryObject &object);

    void Retrieve(uint160 holder_data_hash, MemoryObject &object);
};


#endif //TELEPORT_RELAYMESSAGEHASHDATA_H
