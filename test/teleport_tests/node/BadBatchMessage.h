#ifndef TELEPORT_BADBATCHMESSAGE_H
#define TELEPORT_BADBATCHMESSAGE_H

#include <src/crypto/uint256.h>
#include <src/mining/work.h>
#include "src/base/serialize.h"

class BadBatchMessage
{
public:
    uint160 mined_credit_message_hash;
    TwistWorkCheck check;

    static std::string Type() { return "bad_batch"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit_message_hash);
        READWRITE(check);
    )

    DEPENDENCIES(mined_credit_message_hash);

    HASH160();
};


#endif //TELEPORT_BADBATCHMESSAGE_H
