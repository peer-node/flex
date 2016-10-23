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
    relay_join_message.public_key = Point(SECP256K1, 2);
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    vector<Relay> relays = relay_state.relays;
    ASSERT_THAT(relays.size(), Eq(1));
    ASSERT_THAT(relays[0].public_key, Eq(relay_join_message.public_key));
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

class ARelayStateWithFiveRelays : public ARelayState
{
public:
    virtual void SetUp()
    {
        for (uint32_t i = 1; i <= 5; i ++)
        {
            RelayJoinMessage join_message;
            join_message.public_key = Point(SECP256K1, i);
            relay_state.ProcessRelayJoinMessage(join_message);
        }
    }
};

TEST_F(ARelayStateWithFiveRelays, AssignsQuarterKeyHoldersToARelay)
{
    Relay& relay = relay_state.relays[4];
    relay_state.AssignQuarterKeyHoldersToRelay(relay);

    vector<uint64_t> quarter_key_holders_for_relay1 = relay.quarter_key_holders;
    ASSERT_THAT(quarter_key_holders_for_relay1, Eq(vector<uint64_t>{6, 4, 3, 2}));
}