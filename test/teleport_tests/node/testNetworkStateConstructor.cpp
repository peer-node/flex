#include <test/teleport_tests/mined_credits/MinedCredit.h>
#include <src/crypto/point.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "gmock/gmock.h"
#include "NetworkStateConstructor.h"

using namespace ::testing;
using namespace std;


class ANetworkStateConstructor : public Test
{
public:
    NetworkStateConstructor *network_state_constructor;
    MemoryDataStore msgdata, creditdata;

    virtual void SetUp()
    {
        network_state_constructor = new NetworkStateConstructor(msgdata, creditdata);
    }

    virtual void TearDown()
    {
        delete network_state_constructor;
    }
};

TEST_F(ANetworkStateConstructor, ConstructsANetworkStateWithTheRightNetworkId)
{
    TeleportConfig config;
    config["network_id"] = "1";
    network_state_constructor->SetConfig(config);
    EncodedNetworkState network_state = network_state_constructor->ConstructNetworkState();
    ASSERT_THAT(network_state.network_id, Eq(1));
}

TEST_F(ANetworkStateConstructor, InitiallyConstructsANetworkStateWithAPreviousMinedCreditHashOfZero)
{
    EncodedNetworkState network_state = network_state_constructor->ConstructNetworkState();
    ASSERT_THAT(network_state.previous_mined_credit_message_hash, Eq(0));
}

