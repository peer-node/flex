#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "gmock/gmock.h"
#include "RelayState.h"
#include "Relay.h"
#include "RelayJoinMessage.h"
#include "KeyQuartersMessage.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"

TEST(ARelay, GeneratesAKeyQuartersMessageAndStoresTheCorrespondingFourSecrets)
{
    MemoryDataStore keydata;
    Relay relay;
    KeyQuartersMessage key_quarters_message = relay.GenerateKeyQuartersMessage(keydata);

    ASSERT_THAT(key_quarters_message.public_key_quarters.size(), Eq(4));

    for (auto public_key_quarter : key_quarters_message.public_key_quarters)
        ASSERT_TRUE(keydata[public_key_quarter].HasProperty("privkey"));
}

TEST(ARelay, GeneratesAKeyQuartersMessageSpecifyingTheRelaysJoinMessageHash)
{
    MemoryDataStore keydata;
    Relay relay;
    relay.join_message_hash = 5;
    KeyQuartersMessage key_quarters_message = relay.GenerateKeyQuartersMessage(keydata);

    ASSERT_THAT(key_quarters_message.join_message_hash, Eq(relay.join_message_hash));

}