
#ifndef TELEPORT_RELAYSTATE_H
#define TELEPORT_RELAYSTATE_H


#include <vector>
#include "Relay.h"
#include "RelayJoinMessage.h"

class RelayState
{
public:
    std::vector<Relay> relays;
    std::map<uint64_t, Relay&> relays_by_number;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relays);
    )

    JSON(relays);

    HASH160();

    void ProcessRelayJoinMessage(RelayJoinMessage relay_join_message);

    Relay GenerateRelayFromRelayJoinMessage(RelayJoinMessage relay_join_message);

    Relay& GetRelayByNumber(uint64_t number);

    void AssignRemainingQuarterKeyHoldersToRelay(Relay &relay, uint160 encoding_message_hash);

    bool IsDead(uint64_t relay_number);

    bool AssignKeyPartHoldersToRelay(Relay &relay, uint160 encoding_message_hash);

    uint64_t NumberOfRelaysThatJoinedLaterThan(Relay &relay);

    uint64_t NumberOfRelaysThatJoinedBefore(Relay &relay);

    bool ThereAreEnoughRelaysToAssignKeyPartHolders(Relay &relay);

    uint64_t KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed);

    bool KeyPartHolderIsAlreadyBeingUsed(Relay &key_distributor, Relay &key_part_holder);

    uint64_t SelectKeyHolderWhichIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed);

    void AssignRemainingKeySixteenthHoldersToRelay(Relay &relay, uint160 encoding_message_hash);

    void AssignKeySixteenthAndQuarterHoldersWhoJoinedLater(Relay &relay, uint160 encoding_message_hash);

    void RemoveKeyPartHolders(Relay &relay);

    std::vector<Point> GetKeyQuarterHoldersAsListOf16Recipients(uint64_t relay_number);

    std::vector<Point> GetKeySixteenthHoldersAsListOf16Recipients(uint64_t relay_number, uint64_t first_or_second_set);
};


#endif //TELEPORT_RELAYSTATE_H
