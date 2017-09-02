#ifndef TELEPORT_KEYDISTRIBUTIONAUDITORSELECTION_H
#define TELEPORT_KEYDISTRIBUTIONAUDITORSELECTION_H


#include <cstdint>
#include <src/base/serialize.h>
#include <test/teleport_tests/node/relays/messages/KeyDistributionMessage.h>
#include "define.h"
#include <array>

class RelayState;

class KeyDistributionAuditorSelection
{
public:
    std::array<uint64_t, 24> first_set_of_auditors;
    std::array<uint64_t, 24> second_set_of_auditors;
    bool failed_to_assign_auditors{true};

    IMPLEMENT_SERIALIZE
    (
        READWRITE(first_set_of_auditors);
        READWRITE(second_set_of_auditors);
    );

    JSON(first_set_of_auditors, second_set_of_auditors);

    bool operator==(const KeyDistributionAuditorSelection &other) const
    {
        return first_set_of_auditors == other.first_set_of_auditors and
               second_set_of_auditors == other.second_set_of_auditors;
    }

    uint64_t NumberOfEligibleRelaysWhoJoinedLaterThan(Relay *key_sharer, RelayState *state);

    uint64_t NumberOfEligibleRelays(Relay *key_sharer, RelayState *state);

    bool RelayIsEligibleToBeAnAuditor(Relay *relay, Relay *key_sharer);

    std::vector<Relay *> EligibleRelaysWhoJoinedLaterThan(Relay *key_sharer, RelayState *state);

    void PopulateAuditorsWhoJoinedLaterThanKeySharer(Relay *key_sharer, RelayState *state);

    std::vector<Relay *> SelectRandomRelaysFromList(std::vector<Relay *> list_of_relays, uint64_t number, uint160 chooser);

    std::vector<Relay *> EligibleRelays(Relay *key_sharer, RelayState *state);

    void RemoveRelayFromList(uint64_t relay_number, std::vector<Relay *> &list_of_relays);

    std::vector<Relay *> EligibleRelaysOtherThanFirstTwoFromEachSet(Relay *key_sharer, RelayState *state);

    void PopulateRemainingAuditors(Relay *key_sharer, RelayState *state);

    bool ThereAreEnoughRelaysToChooseAuditors(Relay *key_sharer, RelayState *state);

    void Populate(Relay *key_sharer, RelayState *state);
};


#endif //TELEPORT_KEYDISTRIBUTIONAUDITORSELECTION_H
