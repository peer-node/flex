#ifndef TELEPORT_KEYQUARTERLOCATION_H
#define TELEPORT_KEYQUARTERLOCATION_H

#include <src/base/serialize.h>
#include <src/crypto/uint256.h>
#include <src/define.h>

class KeyQuarterLocation
{
public:
    uint160 message_hash{0};
    uint32_t position{0};
    bool message_was_encoded{0};
    bool key_quarter_was_recovered{0};
    uint160 audit_message_hash{0};
    bool message_was_audited{0};

    IMPLEMENT_SERIALIZE
    (
        READWRITE(message_hash);
        READWRITE(position);
        READWRITE(message_was_encoded);
        READWRITE(audit_message_hash);
        READWRITE(message_was_audited);
    );

    JSON(message_hash, position, message_was_encoded, audit_message_hash, message_was_audited);
};


#endif //TELEPORT_KEYQUARTERLOCATION_H
