#ifndef TELEPORT_RELAYMESSAGEHASHDATA_H
#define TELEPORT_RELAYMESSAGEHASHDATA_H


#include <src/crypto/uint256.h>
#include <test/teleport_tests/teleport_data/MemoryObject.h>


class RelayMessageHashData
{
public:
    uint160 join_message_hash{0};
    uint160 goodbye_message_hash{0};
    uint160 mined_credit_message_hash{0};
    uint160 key_distribution_audit_message_hash{0};
    std::vector<uint160> key_distribution_complaint_hashes;
    std::vector<uint160> hashes_of_messages_not_responded_to;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(join_message_hash);
        READWRITE(goodbye_message_hash);
        READWRITE(mined_credit_message_hash);
        READWRITE(key_distribution_audit_message_hash);
        READWRITE(key_distribution_complaint_hashes);
        READWRITE(hashes_of_messages_not_responded_to);
    );

    JSON(join_message_hash, goodbye_message_hash, mined_credit_message_hash,
         key_distribution_complaint_hashes, hashes_of_messages_not_responded_to);

    HASH160();

};


#endif //TELEPORT_RELAYMESSAGEHASHDATA_H
