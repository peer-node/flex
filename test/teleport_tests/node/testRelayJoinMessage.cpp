#include "gmock/gmock.h"
#include "test/teleport_tests/node/relay_handler/RelayState.h"
#include "test/teleport_tests/node/relay_handler/Relay.h"
#include "test/teleport_tests/node/relay_handler/RelayJoinMessage.h"
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

TEST_F(ARelayJoinMessage, GeneratesKeySixteenthsWhichSumToTheRelaysPublicSigningKey)
{
    relay_join_message.public_key_set.Populate(keydata);

    Point signing_key = relay_join_message.PublicSigningKey();
    ASSERT_THAT(SumOfPoints(relay_join_message.public_key_set.PublicKeySixteenths()), Eq(signing_key));
}

TEST_F(ARelayJoinMessage, StoresThePrivateKeyWhenGeneratingThePublicKeySet)
{
    relay_join_message.PopulatePublicKeySet(keydata);
    Point public_key = relay_join_message.PublicSigningKey();
    CBigNum private_key = keydata[public_key]["privkey"];
    ASSERT_THAT(Point(private_key), Eq(public_key));
}

TEST_F(ARelayJoinMessage, PopulatesPrivateKeySixteenthsEncryptedToThePublicKey)
{
    relay_join_message.PopulatePublicKeySet(keydata);
    relay_join_message.PopulatePrivateKeySixteenths(keydata);
    ASSERT_THAT(relay_join_message.encrypted_private_key_sixteenths.size(), Eq(16));

    auto public_key = relay_join_message.PublicSigningKey();
    CBigNum private_key = keydata[public_key]["privkey"];

    for (uint32_t i = 0; i < relay_join_message.encrypted_private_key_sixteenths.size(); i ++)
    {
        CBigNum shared_secret = Hash(private_key * relay_join_message.public_key_set.PublicKeySixteenths()[i]);
        CBigNum private_key_sixteenth = shared_secret ^ CBigNum(relay_join_message.encrypted_private_key_sixteenths[i]);
        ASSERT_THAT(Point(private_key_sixteenth), Eq(relay_join_message.public_key_set.PublicKeySixteenths()[i]));
    }
}

TEST_F(ARelayJoinMessage, ValidatesTheSizesOfTheEnclosedData)
{
    relay_join_message.PopulatePublicKeySet(keydata);
    relay_join_message.PopulatePrivateKeySixteenths(keydata);

    bool ok = relay_join_message.ValidateSizes();
    ASSERT_THAT(ok, Eq(true));

    relay_join_message.encrypted_private_key_sixteenths.push_back(uint256(0));
    ASSERT_THAT(relay_join_message.ValidateSizes(), Eq(false));

    relay_join_message.encrypted_private_key_sixteenths.pop_back();
    relay_join_message.public_key_set.key_points[0].push_back(Point(CBigNum(0)));
    ASSERT_THAT(relay_join_message.ValidateSizes(), Eq(false));
}