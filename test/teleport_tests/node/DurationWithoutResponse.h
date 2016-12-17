#ifndef TELEPORT_DURATIONWITHOUTRESPONSE_H
#define TELEPORT_DURATIONWITHOUTRESPONSE_H

#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include <src/define.h>

#define NETWORK_DIAMETER 4 // seconds
#define TWICE_NETWORK_DIAMETER 8

class DurationWithoutResponse
{
public:
    uint160 message_hash{0};
    uint64_t seconds_since_message{TWICE_NETWORK_DIAMETER};

    static std::string Type() { return "duration_without_response"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(message_hash);
        READWRITE(seconds_since_message);
    )

    JSON(message_hash, seconds_since_message);

    DEPENDENCIES(message_hash);

    HASH160();
};


#endif //TELEPORT_DURATIONWITHOUTRESPONSE_H
