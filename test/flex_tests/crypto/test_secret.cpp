#include "gmock/gmock.h"
#include "crypto/point.h"
#include "DistributedSecret.h"
#include "EncryptedSecret.h"

using namespace ::testing;

TEST(ADistributedSecret, StartsWithZeroProbabilityOfDiscovery)
{
    DistributedSecret s;
    ASSERT_THAT(s.probability_of_discovery(1.0), Eq(0.0));
}

TEST(ADistributedSecret_KnownToOneRelay, HasProbabilityOfDiscoveryEqualToFractionConspiring)
{
    DistributedSecret s;
    s.RevealToOneRelay();
    ASSERT_DOUBLE_EQ(0.3, s.probability_of_discovery(0.3));
    ASSERT_DOUBLE_EQ(0.4, s.probability_of_discovery(0.4));
}

TEST(ADistributedSecret_KnownToTwoRelays, HasProbabilityEqualToProbabilityThatAtLeastOneKnows)
{
    DistributedSecret s;
    s.RevealToOneRelay();
    s.RevealToOneRelay();
    ASSERT_DOUBLE_EQ(0.3 + (1 - 0.3) * 0.3, s.probability_of_discovery(0.3));
}

TEST(ADistributedSecret_SplitAmongTwoRelays, HasProbabilityEqualToSquareOfFractionConspiring)
{
    DistributedSecret s;
    s.SplitAmongRelays(2);
    ASSERT_DOUBLE_EQ(0.3 * 0.3, s.probability_of_discovery(0.3));
    ASSERT_DOUBLE_EQ(0.4 * 0.4, s.probability_of_discovery(0.4));
}

TEST(ADistributedSecret_SplitAmongThreeRelays, HasProbabilityEqualToCubeOfFractionConspiring)
{
    DistributedSecret s;
    s.SplitAmongRelays(3);
    ASSERT_DOUBLE_EQ(0.3 * 0.3 * 0.3, s.probability_of_discovery(0.3));
    ASSERT_DOUBLE_EQ(0.4 * 0.4 * 0.4, s.probability_of_discovery(0.4));
}

TEST(ADistributedSecret_KnownToOneRelayAndSplitAmongTwo,
     HasProbabilityEqualToLogicalOrOfTwoDiscoveryPossibilities)
{
    DistributedSecret s;
    s.RevealToOneRelay();
    s.SplitAmongRelays(2);
    ASSERT_DOUBLE_EQ(0.3 + (1 - 0.3) * 0.3 * 0.3, s.probability_of_discovery(0.3));
}

TEST(ADistributedSecret_SplitAmongTwoRelaysTwice,
     HasProbabilityEqualToLogicalOrOfTwoDiscoveryPossibilities)
{
    DistributedSecret s;
    s.SplitAmongRelays(2);
    s.SplitAmongRelays(2);
    ASSERT_DOUBLE_EQ(2 * pow(0.3, 2) - pow(0.3, 4), s.probability_of_discovery(0.3));
}


class AnEncryptedSecret : public Test
{
public:
    EncryptedSecret encrypted_secret;
    CBigNum recipient_private_key;
    CBigNum secret;
    Point recipient;

    virtual void SetUp()
    {
        recipient_private_key = 12345;
        secret = 54321;
        recipient = Point(SECP256K1, recipient_private_key);
        encrypted_secret = EncryptedSecret(secret, recipient);
    }

};

TEST_F(AnEncryptedSecret, SpecifiesTheCorrespondingEllipticCurvePoint)
{
    Point point = encrypted_secret.point;
}

TEST_F(AnEncryptedSecret, SpecifiesTheSecretXorSharedSecret)
{
    CBigNum secret_xor_shared_secret = encrypted_secret.secret_xor_shared_secret;
}

TEST_F(AnEncryptedSecret, YieldsSecretToRecipient)
{
    CBigNum recovered_secret = encrypted_secret.RecoverSecret(recipient_private_key);
    ASSERT_THAT(recovered_secret, Eq(secret));
}

TEST_F(AnEncryptedSecret, CanBeAuditedUsingSecret)
{
    ASSERT_TRUE(encrypted_secret.Audit(secret, recipient));
    ASSERT_FALSE(encrypted_secret.Audit(secret + 1, recipient));
    ASSERT_FALSE(encrypted_secret.Audit(secret, 2 * recipient));
}

TEST_F(AnEncryptedSecret, FailsAuditIfPointDoesNotMatchSecret)
{
    Point bad_point(SECP256K1, 98765);
    encrypted_secret.point = bad_point;
    ASSERT_FALSE(encrypted_secret.Audit(secret, recipient));
}

