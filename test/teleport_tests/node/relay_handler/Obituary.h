#ifndef TELEPORT_OBITUARY_H
#define TELEPORT_OBITUARY_H

#include "Relay.h"

#define OBITUARY_SAID_GOODBYE 1
#define OBITUARY_NOT_RESPONDING 2
#define OBITUARY_COMPLAINT 3


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
