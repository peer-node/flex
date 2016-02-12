#include "gmock/gmock.h"
#include "crypto/point.h"
#include "DistributedSecret.h"

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
