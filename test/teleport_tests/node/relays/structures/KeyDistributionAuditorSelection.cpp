#include <src/credits/creditsign.h>
#include "KeyDistributionAuditorSelection.h"
#include "RelayState.h"

using std::vector;

#include "log.h"
#define LOG_CATEGORY "KeyDistributionAuditorSelection.cpp"


void KeyDistributionAuditorSelection::Populate(Relay *key_sharer, RelayState *state)
{
    if (not ThereAreEnoughRelaysToChooseAuditors(key_sharer, state))
    {
        failed_to_assign_auditors = true;
        return;
    }
    PopulateAuditorsWhoJoinedLaterThanKeySharer(key_sharer, state);
    PopulateRemainingAuditors(key_sharer, state);
    failed_to_assign_auditors = false;
}

bool KeyDistributionAuditorSelection::ThereAreEnoughRelaysToChooseAuditors(Relay *key_sharer, RelayState *state)
{
    uint64_t number_of_eligible_relays = NumberOfEligibleRelays(key_sharer, state);
    uint64_t number_of_later_relays = NumberOfEligibleRelaysWhoJoinedLaterThan(key_sharer, state);

    return number_of_later_relays >= 2 and number_of_eligible_relays >= 48;
}

uint64_t KeyDistributionAuditorSelection::NumberOfEligibleRelaysWhoJoinedLaterThan(Relay *key_sharer, RelayState *state)
{
    return EligibleRelaysWhoJoinedLaterThan(key_sharer, state).size();
}

vector<Relay*> KeyDistributionAuditorSelection::EligibleRelaysWhoJoinedLaterThan(Relay *key_sharer, RelayState *state)
{
    vector<Relay*> eligible_relays;
    for (auto &relay : state->relays)
        if (relay.number > key_sharer->number and RelayIsEligibleToBeAnAuditor(&relay, key_sharer))
            eligible_relays.push_back(&relay);
    return eligible_relays;
}

uint64_t KeyDistributionAuditorSelection::NumberOfEligibleRelays(Relay *key_sharer, RelayState *state)
{
    return EligibleRelays(key_sharer, state).size();
}

vector<Relay*> KeyDistributionAuditorSelection::EligibleRelays(Relay *key_sharer, RelayState *state)
{
    vector<Relay*> eligible_relays;
    for (auto &relay : state->relays)
        if (RelayIsEligibleToBeAnAuditor(&relay, key_sharer))
            eligible_relays.push_back(&relay);
    return eligible_relays;
}

bool KeyDistributionAuditorSelection::RelayIsEligibleToBeAnAuditor(Relay *relay, Relay *key_sharer)
{
    if (relay->status != ALIVE)
        return false;
    if (relay->number == key_sharer->number)
        return false;
    if (ArrayContainsEntry(key_sharer->key_quarter_holders, relay->number))
        return false;;
    return true;
}

void KeyDistributionAuditorSelection::PopulateAuditorsWhoJoinedLaterThanKeySharer(Relay *key_sharer, RelayState *state)
{
    auto eligible_relays = EligibleRelaysWhoJoinedLaterThan(key_sharer, state);
    auto chosen_relays = SelectRandomRelaysFromList(eligible_relays, 2, state->latest_mined_credit_message_hash);
    first_set_of_auditors[0] = chosen_relays[0]->number;
    second_set_of_auditors[0] = chosen_relays[1]->number;
}

vector<Relay*> KeyDistributionAuditorSelection::SelectRandomRelaysFromList(vector<Relay*> list_of_relays,
                                                                           uint64_t number, uint160 chooser)
{
    vector<Relay*> selected_relays;
    while (selected_relays.size() < number)
    {
        chooser = HashUint160(chooser);
        auto selected_relay = list_of_relays[chooser.GetLow64() % list_of_relays.size()];
        selected_relays.push_back(selected_relay);
        EraseEntryFromVector(selected_relay, list_of_relays);
    }
    return selected_relays;
}

void KeyDistributionAuditorSelection::RemoveRelayFromList(uint64_t relay_number, vector<Relay*> &list_of_relays)
{
    for (auto relay : list_of_relays)
        if (relay->number == relay_number)
            EraseEntryFromVector(relay, list_of_relays);
}

vector<Relay*> KeyDistributionAuditorSelection::EligibleRelaysOtherThanFirstTwoFromEachSet(Relay *key_sharer,
                                                                                           RelayState *state)
{
    auto eligible_relays = EligibleRelays(key_sharer, state);
    RemoveRelayFromList(first_set_of_auditors[0], eligible_relays);
    RemoveRelayFromList(second_set_of_auditors[0], eligible_relays);
    return eligible_relays;
}

void KeyDistributionAuditorSelection::PopulateRemainingAuditors(Relay *key_sharer, RelayState *state)
{
    auto eligible_relays = EligibleRelaysOtherThanFirstTwoFromEachSet(key_sharer, state);
    auto chosen_relays = SelectRandomRelaysFromList(eligible_relays, 46, state->latest_mined_credit_message_hash);
    for (uint64_t i = 0; i < 23; i++)
    {
        first_set_of_auditors[i + 1] = chosen_relays[i]->number;
        second_set_of_auditors[i + 1] = chosen_relays[23 + i]->number;
    }
}
