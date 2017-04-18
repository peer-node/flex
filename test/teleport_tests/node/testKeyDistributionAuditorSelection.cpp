#include <src/vector_tools.h>
#include <src/base/util_time.h>
#include <src/base/util_rand.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/relays/RelayState.h"
#include "test/teleport_tests/node/relays/KeyDistributionAuditorSelection.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class AKeyDistributionAuditorSelection : public Test
{
public:
    RelayState state;
    Data *data;
    MemoryDataStore msgdata, creditdata, keydata;
    KeyDistributionAuditorSelection auditor_selection;

    virtual void SetUp()
    {
        data = new Data(msgdata, creditdata, keydata);
        AssignValuesToAuditorSelection();
        PopulateRelayState();
    }

    void AssignValuesToAuditorSelection()
    {
        for (uint64_t i = 0; i < 24; i++)
        {
            auditor_selection.first_set_of_auditors[i] = i;
            auditor_selection.second_set_of_auditors[i] = 24 + i;
        }
    }

    void PopulateRelayState()
    {
        for (uint64_t i = 0; i < 60; i++)
            state.ProcessRelayJoinMessage(Relay().GenerateJoinMessage(data->keydata, i));
    }

    KeyDistributionMessage GetKeyDistributionMessage(Relay &relay)
    {
        KeyDistributionMessage key_distribution_message;
        key_distribution_message.relay_number = relay.number;
        key_distribution_message.relay_join_hash = relay.hashes.join_message_hash;
        key_distribution_message.hash_of_message_containing_join = relay.hashes.join_message_hash + 1;
        return key_distribution_message;
    }

    RelayState MarkSomeEligibleRelaysAsDead(uint64_t number_to_kill, Relay *key_sharer)
    {
        RelayState state_ = state;
        while (number_to_kill > 0)
        {
            for (auto &relay : state_.relays)
                if (relay.status == ALIVE and relay.number != key_sharer->number and
                        not ArrayContainsEntry(key_sharer->key_quarter_holders, relay.number))
                {
                    relay.status = MISBEHAVED;
                    number_to_kill -= 1;
                    if (number_to_kill == 0)
                        break;
                }
        }
        return state_;
    }

    virtual void TearDown()
    {
        delete data;
    }
};

TEST_F(AKeyDistributionAuditorSelection, RetainsItsValuesAfterSerialization)
{
    KeyDistributionAuditorSelection retrieved_selection;

    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << auditor_selection;
    ss >> retrieved_selection;

    for (uint64_t i = 0; i < 24; i++)
    {
        ASSERT_THAT(retrieved_selection.first_set_of_auditors[i], Eq(i)) << "failed at " << i;
        ASSERT_THAT(retrieved_selection.second_set_of_auditors[i], Eq(24 + i)) << "failed at " << i + 24;
    }
}

TEST_F(AKeyDistributionAuditorSelection, IsEqualToAnotherAuditorSelectionWithTheSameValues)
{
    KeyDistributionAuditorSelection retrieved_selection;

    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << auditor_selection;
    ss >> retrieved_selection;

    ASSERT_THAT(retrieved_selection, Eq(auditor_selection));
}

TEST_F(AKeyDistributionAuditorSelection, ChecksThatThereAreEnoughRelaysAvailableToBeAuditors)
{
    for (auto &relay : state.relays)
    {
        auto key_distribution_message = GetKeyDistributionMessage(relay);
        state.ProcessKeyDistributionMessage(key_distribution_message);
        bool enough_relays = auditor_selection.ThereAreEnoughRelaysToChooseAuditors(&relay, &state);
        ASSERT_THAT(enough_relays, Eq(relay.number <= 57)) << "failed at " << relay.number;
    }

    for (auto &relay : state.relays)
    {
        auto key_distribution_message = GetKeyDistributionMessage(relay);
        auto number_of_relays_to_kill = GetRand(30);
        auto state_ = MarkSomeEligibleRelaysAsDead(number_of_relays_to_kill, &relay);
        bool enough_relays = auditor_selection.ThereAreEnoughRelaysToChooseAuditors(&relay, &state_);
        ASSERT_THAT(enough_relays, Eq(relay.number <= 57 and number_of_relays_to_kill < 8)) << "failed at " << relay.number;
    }
}

TEST_F(AKeyDistributionAuditorSelection, FailsToPopulateIfThereAreNotEnoughRelaysAvailableToBeAuditors)
{
    for (auto &relay : state.relays)
    {
        auto key_distribution_message = GetKeyDistributionMessage(relay);
        state.ProcessKeyDistributionMessage(key_distribution_message);

        bool enough_relays = auditor_selection.ThereAreEnoughRelaysToChooseAuditors(&relay, &state);
        auditor_selection.Populate(&relay, &state);
        ASSERT_THAT(auditor_selection.failed_to_assign_auditors, Eq(not enough_relays)) << "failed at " << relay.number;
    }
}