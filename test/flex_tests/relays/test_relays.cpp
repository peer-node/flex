#include "gmock/gmock.h"
#include "../flex_data/TestData.h"
#include "RelayState_.h"
 
using namespace ::testing;

class ARelay : public TestWithGlobalDatabases
{
public:
    Relay relay;
};

TEST_F(ARelay, HasZeroMinedCreditHashByDefault)
{
    ASSERT_THAT(relay.mined_credit_hash, Eq(0));
}

TEST_F(ARelay, HasNumberZeroByDefault)
{
    ASSERT_THAT(relay.number, Eq(0));
}

TEST_F(ARelay, CanBeStoredInADatabase)
{
    relaydata["example"]["relay"] = relay;
}

TEST_F(ARelay, RetainsDataWhenRetrievedFromDatabase)
{
    relay.mined_credit_hash = 1;
    relaydata["example"]["relay"] = relay;
    Relay relay2;
    relay2 = relaydata["example"]["relay"];
    ASSERT_THAT(relay2.mined_credit_hash, Eq(1));
}

class ARelayState_ : public TestWithGlobalDatabases
{
public:
    RelayState_ state;
};

TEST_F(ARelayState_, HasNoRelaysByDefault)
{
    ASSERT_THAT(state.relays.size(), Eq(0));
}

