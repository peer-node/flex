#include <src/vector_tools.h>
#include "gmock/gmock.h"
#include "RelayState.h"
#include "Relay.h"
#include "RelayJoinMessage.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ARelayState : public Test
{
public:
    RelayState relay_state;
    RelayJoinMessage relay_join_message;

    virtual void SetUp() { }
    virtual void TearDown() { }
};

TEST_F(ARelayState, InitiallyHasNoRelays)
{
    vector<Relay> relays = relay_state.relays;
    ASSERT_THAT(relays.size(), Eq(0));
}

TEST_F(ARelayState, AddsARelayWhenProcessingARelayJoinMessage)
{
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    vector<Relay> relays = relay_state.relays;
    ASSERT_THAT(relays.size(), Eq(1));
}

TEST_F(ARelayState, AssignsARelayNumberToAJoiningRelay)
{
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    ASSERT_THAT(relay_state.relays[0].number, Eq(1));
}

TEST_F(ARelayState, PopulatesTheRelaysJoinMessageHash)
{
    RelayState relay_state;
    RelayJoinMessage relay_join_message;
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    uint160 join_message_hash = relay_state.relays[0].join_message_hash;
    ASSERT_THAT(join_message_hash, Eq(relay_join_message.GetHash160()));
}

class ARelayStateWith37Relays : public ARelayState
{
public:
    virtual void SetUp()
    {
        for (uint32_t i = 1; i <= 37; i ++)
        {
            RelayJoinMessage join_message;
            relay_state.ProcessRelayJoinMessage(join_message);
        }
    }
};

bool VectorContainsANumberGreaterThan(std::vector<uint64_t> numbers, uint64_t given_number)
{
    for (auto number : numbers)
        if (number > given_number)
            return true;
    return false;
}

TEST_F(ARelayStateWith37Relays, AssignsKeyPartHoldersToEachRelayWithThreeRelaysWhoJoinedLaterThanIt)
{
    uint160 encoding_message_hash = 1;
    for (auto relay : relay_state.relays)
    {
        bool holders_assigned = relay_state.AssignKeyPartHoldersToRelay(relay, encoding_message_hash);
        ASSERT_THAT(holders_assigned, Eq(relay.number <= 34));
    }
}

TEST_F(ARelayStateWith37Relays, AssignsFourQuarterKeyHoldersAtLeastOneOfWhichJoinedLaterThanTheGivenRelay)
{
    for (auto relay : relay_state.relays)
    {
        if (relay_state.AssignKeyPartHoldersToRelay(relay, 1))
        {
            ASSERT_THAT(relay.quarter_key_holders.size(), Eq(4));
            ASSERT_TRUE(VectorContainsANumberGreaterThan(relay.quarter_key_holders, relay.number));
        }
    }
}

TEST_F(ARelayStateWith37Relays, AssignsKeySixteenthHoldersAtLeastOneOfWhichJoinedLaterThanTheGivenRelay)
{
    for (auto relay : relay_state.relays)
    {
        if (relay_state.AssignKeyPartHoldersToRelay(relay, 1))
        {
            ASSERT_THAT(relay.first_set_of_key_sixteenth_holders.size(), Eq(16));
            ASSERT_TRUE(VectorContainsANumberGreaterThan(relay.first_set_of_key_sixteenth_holders, relay.number));
            ASSERT_THAT(relay.second_set_of_key_sixteenth_holders.size(), Eq(16));
            ASSERT_TRUE(VectorContainsANumberGreaterThan(relay.second_set_of_key_sixteenth_holders, relay.number));
        }
    }
}

bool RelayHasDistinctKeyPartHolders(Relay &relay)
{
    std::set<uint64_t> key_part_holders;
    for (auto key_holder_group : relay.KeyPartHolderGroups())
        for (auto key_part_holder : key_holder_group)
            if (key_part_holders.count(key_part_holder))
                return false;
            else
                key_part_holders.insert(key_part_holder);
    return true;
}

TEST_F(ARelayStateWith37Relays, Assigns36DistinctKeyPartHolders)
{
    for (auto relay : relay_state.relays)
    {
        if (relay_state.AssignKeyPartHoldersToRelay(relay, 1))
        {
            ASSERT_TRUE(RelayHasDistinctKeyPartHolders(relay));
        }
    }
}

TEST_F(ARelayStateWith37Relays, DoesntAssignARelayToHoldItsOwnKeyParts)
{
    for (auto relay : relay_state.relays)
    {
        relay_state.AssignKeyPartHoldersToRelay(relay, 1);
        for (auto key_holder_group : relay.KeyPartHolderGroups())
            ASSERT_THAT(VectorContainsEntry(key_holder_group, relay.number), Eq(false));
    }
}