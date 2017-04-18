#include "gmock/gmock.h"
#include "test/teleport_tests/node/relays/RelayState.h"
#include "test/teleport_tests/node/relays/Relay.h"
#include "test/teleport_tests/node/relays/RelayJoinMessage.h"
#include "crypto/bignum_hashes.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ARelayJoinMessage : public Test
{
public:
    MemoryDataStore msgdata, creditdata, keydata;
    RelayJoinMessage relay_join_message;
};

TEST_F(ARelayJoinMessage, GeneratesKeyTwentyFourthsWhichSumToTheRelaysPublicSigningKey)
{
    relay_join_message.public_key_set.Populate(keydata);

    Point signing_key = relay_join_message.PublicSigningKey();
    ASSERT_THAT(SumOfPoints(relay_join_message.public_key_set.PublicKeyTwentyFourths()), Eq(signing_key));
}

TEST_F(ARelayJoinMessage, StoresThePrivateKeyWhenGeneratingThePublicKeySet)
{
    relay_join_message.PopulatePublicKeySet(keydata);
    Point public_key = relay_join_message.PublicSigningKey();
    CBigNum private_key = keydata[public_key]["privkey"];
    ASSERT_THAT(Point(private_key), Eq(public_key));
}

TEST_F(ARelayJoinMessage, ValidatesTheSizesOfTheEnclosedData)
{
    relay_join_message.PopulatePublicKeySet(keydata);

    bool ok = relay_join_message.ValidateSizes();
    ASSERT_THAT(ok, Eq(true));

    relay_join_message.public_key_set.key_points[0].push_back(Point(CBigNum(0)));
    ASSERT_THAT(relay_join_message.ValidateSizes(), Eq(false));
}