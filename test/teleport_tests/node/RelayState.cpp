#include <src/vector_tools.h>
#include "RelayState.h"

void RelayState::ProcessRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    Relay relay = GenerateRelayFromRelayJoinMessage(relay_join_message);
    relays.push_back(relay);
}

Relay RelayState::GenerateRelayFromRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    Relay new_relay;
    if (relays.size() == 0)
        new_relay.number = 1;
    else
        new_relay.number = relays.back().number + 1;

    new_relay.public_key = relay_join_message.public_key;

    new_relay.join_message_hash = relay_join_message.GetHash160();
    return new_relay;
}

void RelayState::AssignQuarterKeyHoldersToRelay(Relay &relay)
{
    if (relay.number < 5)
        return;
    uint64_t quarter_key_holder = relay.number + 1;
    relay.quarter_key_holders.resize(0);

    relay.quarter_key_holders.push_back(quarter_key_holder);
    while (relay.quarter_key_holders.size() < 4)
    {
        quarter_key_holder = relay.number - 1;
        while (IsDead(quarter_key_holder) or VectorContainsEntry(relay.quarter_key_holders, quarter_key_holder))
            quarter_key_holder--;
        relay.quarter_key_holders.push_back(quarter_key_holder);
    }
}

bool RelayState::IsDead(uint64_t relay_number)
{
    return false;
}
