#include "gmock/gmock.h"
#include "RelayState.h"
#include "Relay.h"
#include "RelayJoinMessage.h"
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

TEST_F(ARelayJoinMessage, GeneratesKeySixteenthsWhichSumToTheRelaysPublicKey)
{
    relay_join_message.GenerateKeySixteenths(keydata);

    Point public_key = relay_join_message.PublicKey();
    ASSERT_THAT(SumOfPoints(relay_join_message.public_key_sixteenths), Eq(public_key));
}

TEST_F(ARelayJoinMessage, StoresThePrivateKeyWhenGeneratingKeySixteenths)
{
    relay_join_message.GenerateKeySixteenths(keydata);
    Point public_key = relay_join_message.PublicKey();
    CBigNum private_key = keydata[public_key]["privkey"];
    ASSERT_THAT(Point(SECP256K1, private_key), Eq(public_key));
}

TEST_F(ARelayJoinMessage, PopulatesPrivateKeySixteenthsEncryptedToThePublicKey)
{
    relay_join_message.GenerateKeySixteenths(keydata);
    relay_join_message.PopulatePrivateKeySixteenths(keydata);
    ASSERT_THAT(relay_join_message.encrypted_private_key_sixteenths.size(), Eq(16));

    auto public_key = relay_join_message.PublicKey();
    CBigNum private_key = keydata[public_key]["privkey"];

    for (uint32_t i = 0; i < relay_join_message.encrypted_private_key_sixteenths.size(); i ++)
    {
        CBigNum shared_secret = Hash(private_key * relay_join_message.public_key_sixteenths[i]);
        CBigNum private_key_sixteenth = shared_secret ^ relay_join_message.encrypted_private_key_sixteenths[i];
        ASSERT_THAT(Point(SECP256K1, private_key_sixteenth), Eq(relay_join_message.public_key_sixteenths[i]));
    }
}
