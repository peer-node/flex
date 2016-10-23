
#ifndef TELEPORT_RELAYSTATE_H
#define TELEPORT_RELAYSTATE_H


#include <vector>
#include "Relay.h"
#include "RelayJoinMessage.h"

class RelayState
{
public:
    std::vector<Relay> relays;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relays);
    )

    JSON(relays);

    HASH160();

    void ProcessRelayJoinMessage(RelayJoinMessage relay_join_message);

    Relay GenerateRelayFromRelayJoinMessage(RelayJoinMessage relay_join_message);

    void AssignQuarterKeyHoldersToRelay(Relay &relay);

    bool IsDead(uint64_t relay_number);
};


#endif //TELEPORT_RELAYSTATE_H
