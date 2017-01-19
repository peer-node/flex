#include "gmock/gmock.h"
#include "RelayPublicKeySet.h"
#include "Relay.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ARelayPublicKeySet : public Test
{
public:
    MemoryDataStore keydata;
    RelayPublicKeySet public_key_set;
    Relay relay;
    CBigNum secret_message;

    virtual void SetUp()
    {
        for (uint32_t i = 0; i < 16; i++)
        {
            CBigNum private_key_sixteenth;
            private_key_sixteenth.Randomize(Secp256k1Point::Modulus());
            Point public_key_sixteenth(private_key_sixteenth);
            keydata[public_key_sixteenth]["privkey"] = private_key_sixteenth;
        }
        secret_message.Randomize(Secp256k1Point::Modulus());
    }
};

TEST_F(ARelayPublicKeySet, GeneratesSixteenVectorsWithFourKeyPointsEach)
{
    public_key_set.Populate(keydata);

    ASSERT_THAT(public_key_set.key_points.size(), Eq(16));
    for (auto row_of_key_points : public_key_set.key_points)
        ASSERT_THAT(row_of_key_points.size(), Eq(4));
}

class APopulatedRelayPublicKeySet : public ARelayPublicKeySet
{
public:
    virtual void SetUp()
    {
        ARelayPublicKeySet::SetUp();
        public_key_set.Populate(keydata);
    }
};

TEST_F(APopulatedRelayPublicKeySet, GeneratesAMatchingPublicAndPrivateReceivingKeyForASecretGivenTheCorrespondingPoint)
{
    Point point_corresponding_to_secret(secret_message);
    Point receiving_public_key = public_key_set.GenerateReceivingPublicKey(point_corresponding_to_secret);
    CBigNum receiving_private_key = public_key_set.GenerateReceivingPrivateKey(point_corresponding_to_secret, keydata);

    ASSERT_THAT(Point(receiving_private_key), Eq(receiving_public_key));
}

CBigNum Decrypt(CBigNum encrypted_secret, CBigNum private_key, Point point_corresponding_to_secret)
{
    CBigNum shared_secret = Hash(private_key * point_corresponding_to_secret);
    return encrypted_secret ^ shared_secret;
}

TEST_F(APopulatedRelayPublicKeySet, EncryptsASecretWhichCanBeDecryptedUsingThePrivateReceivingKey)
{
    CBigNum encrypted_secret = public_key_set.Encrypt(secret_message);
    CBigNum receiving_private_key = public_key_set.GenerateReceivingPrivateKey(Point(secret_message), keydata);
    CBigNum decrypted_secret = Decrypt(encrypted_secret, receiving_private_key, Point(secret_message));

    ASSERT_THAT(decrypted_secret, Eq(secret_message));
}

TEST_F(APopulatedRelayPublicKeySet, DecryptsAnEncryptedSecret)
{
    CBigNum encrypted_secret = public_key_set.Encrypt(secret_message);
    CBigNum decrypted_secret = public_key_set.Decrypt(encrypted_secret, keydata, Point(secret_message));

    ASSERT_THAT(decrypted_secret, Eq(secret_message));
}

TEST_F(APopulatedRelayPublicKeySet, ReturnsZeroIfItFailsToDecryptAnEncryptedSecret)
{
    CBigNum encrypted_secret = public_key_set.Encrypt(secret_message);

    encrypted_secret += 1;
    CBigNum decrypted_secret = public_key_set.Decrypt(encrypted_secret, keydata, Point(secret_message));

    ASSERT_THAT(decrypted_secret, Eq(0));
}

TEST_F(APopulatedRelayPublicKeySet, GeneratesReceivingPublicKeyQuartersWhichSumToTheReceivingPublicKey)
{
    Point point_corresponding_to_secret(secret_message);

    auto receiving_key = public_key_set.GenerateReceivingPublicKey(point_corresponding_to_secret);

    auto quarter1 = public_key_set.GenerateReceivingPublicKeyQuarter(point_corresponding_to_secret, 0);
    auto quarter2 = public_key_set.GenerateReceivingPublicKeyQuarter(point_corresponding_to_secret, 1);
    auto quarter3 = public_key_set.GenerateReceivingPublicKeyQuarter(point_corresponding_to_secret, 2);
    auto quarter4 = public_key_set.GenerateReceivingPublicKeyQuarter(point_corresponding_to_secret, 3);

    Point sum = quarter1 + quarter2 + quarter3 + quarter4;

    ASSERT_THAT(sum, Eq(receiving_key));
}

TEST_F(APopulatedRelayPublicKeySet, GeneratesReceivingPrivateKeyQuartersWhichSumToTheReceivingPrivateKey)
{
    Point point_corresponding_to_secret(secret_message);

    auto receiving_key = public_key_set.GenerateReceivingPrivateKey(point_corresponding_to_secret, keydata);

    auto quarter1 = public_key_set.GenerateReceivingPrivateKeyQuarter(point_corresponding_to_secret, 0, keydata);
    auto quarter2 = public_key_set.GenerateReceivingPrivateKeyQuarter(point_corresponding_to_secret, 1, keydata);
    auto quarter3 = public_key_set.GenerateReceivingPrivateKeyQuarter(point_corresponding_to_secret, 2, keydata);
    auto quarter4 = public_key_set.GenerateReceivingPrivateKeyQuarter(point_corresponding_to_secret, 3, keydata);

    CBigNum sum = (quarter1 + quarter2 + quarter3 + quarter4) % Secp256k1Point::Modulus();

    ASSERT_THAT(sum, Eq(receiving_key));
}

TEST_F(APopulatedRelayPublicKeySet, GeneratesMatchingReceivingPrivateAndPublicKeyQuarters)
{
    Point point_corresponding_to_secret(secret_message);

    for (uint8_t i = 0; i < 4; i++)
    {
        auto private_key_quarter = public_key_set.GenerateReceivingPrivateKeyQuarter(point_corresponding_to_secret,
                                                                                     i, keydata);
        auto public_key_quarter = public_key_set.GenerateReceivingPublicKeyQuarter(point_corresponding_to_secret, i);
        ASSERT_THAT(Point(private_key_quarter), Eq(public_key_quarter));
    }
}