#ifndef TELEPORT_OBITUARY_H
#define TELEPORT_OBITUARY_H

#include "crypto/uint256.h"
#include "base/serialize.h"
#include "define.h"

#define SAID_GOODBYE 1
#define NOT_RESPONDING 2
#define MISBEHAVED 3


class Obituary
{
public:
    uint64_t dead_relay_number;
    uint160 relay_state_hash;
    uint64_t successor_number;
    uint32_t reason_for_leaving{0};
    bool in_good_standing{false};

    static std::string Type() { return "obituary"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(dead_relay_number);
        READWRITE(relay_state_hash);
        READWRITE(successor_number);
        READWRITE(reason_for_leaving);
        READWRITE(in_good_standing);
    );

    JSON(dead_relay_number, relay_state_hash, successor_number, reason_for_leaving, in_good_standing);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_OBITUARY_H
