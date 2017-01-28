#ifndef TELEPORT_DURATIONWITHOUTRESPONSEFROMRELAY_H
#define TELEPORT_DURATIONWITHOUTRESPONSEFROMRELAY_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include <src/define.h>
#include "DurationWithoutResponse.h"

class DurationWithoutResponseFromRelay
{
public:
    uint160 message_hash{0};
    uint64_t relay_number{0};
    uint32_t seconds_since_message{TWICE_NETWORK_DIAMETER};

    static std::string Type() { return "duration_without_response_from_relay"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(message_hash);
        READWRITE(relay_number);
        READWRITE(seconds_since_message);
    );

    JSON(message_hash, relay_number, seconds_since_message);

    DEPENDENCIES(message_hash);

    HASH160();
};


#endif //TELEPORT_DURATIONWITHOUTRESPONSEFROMRELAY_H
