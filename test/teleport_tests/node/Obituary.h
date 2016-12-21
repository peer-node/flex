#ifndef TELEPORT_OBITUARY_H
#define TELEPORT_OBITUARY_H

#include "Relay.h"

#define OBITUARY_SAID_GOODBYE 1
#define OBITUARY_NOT_RESPONDING 2
#define OBITUARY_UNREFUTED_COMPLAINT 3   // complaint about relay
#define OBITUARY_REFUTED_COMPLAINT 4     // complaint by relay


class Obituary
{
public:
    Relay relay;
    uint160 relay_state_hash;
    std::vector<std::pair<uint64_t, uint8_t> > key_quarters_held;
    uint64_t successor_number;
    uint32_t reason_for_leaving{0};
    bool in_good_standing{false};

    static std::string Type() { return "obituary"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relay);
        READWRITE(relay_state_hash);
        READWRITE(key_quarters_held);
        READWRITE(successor_number);
        READWRITE(reason_for_leaving);
        READWRITE(in_good_standing);
    );

    JSON(relay, relay_state_hash, key_quarters_held, successor_number, reason_for_leaving, in_good_standing);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_OBITUARY_H
