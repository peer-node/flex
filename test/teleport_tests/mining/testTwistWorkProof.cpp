#include "gmock/gmock.h"
#include "mining/work.h"

using namespace ::testing;

class ATwistWorkProof : public Test
{
public:
    TwistWorkProof proof;
    uint256 memory_seed;
    uint32_t N_factor;
    uint160 target;
    uint160 link_threshold;
    uint32_t num_segments;

    virtual void SetUp()
    {
        memory_seed = 123456789;
        N_factor = 5;
        target = 1;
        target <<= 115;
        link_threshold = 1;
        link_threshold <<= 123;
        num_segments = 16;
        proof = TwistWorkProof(memory_seed, N_factor, target, link_threshold, num_segments);
    }
};

TEST_F(ATwistWorkProof, HasZeroLengthWhenCreated)
{
    ASSERT_THAT(proof.Length(), Eq(0));
}

TEST_F(ATwistWorkProof, DoesTheWork)
{
    uint8_t keep_working = 1;
    proof.DoWork(&keep_working);
    ASSERT_THAT(proof.DifficultyAchieved(), Gt(0));
}

TEST_F(ATwistWorkProof, PassesFullVerification)
{
    uint8_t keep_working = 1;
    proof.DoWork(&keep_working);
    TwistWorkCheck check = proof.CheckRange(0, num_segments, 0, proof.num_links);
    ASSERT_TRUE(check.valid);
}

TEST_F(ATwistWorkProof, PassesSpotChecks)
{
    uint8_t keep_working = 1;
    proof.DoWork(&keep_working);
    ASSERT_TRUE(proof.SpotCheck().valid);
}

TEST_F(ATwistWorkProof, HasAtLeastTenLinks)
{
    proof.link_threshold = proof.target;
    uint8_t keep_working = 1;

    proof.DoWork(&keep_working);
    ASSERT_THAT(proof.links.size(), Gt(9));
    ASSERT_THAT(proof.DifficultyAchieved(), Gt(0));
    TwistWorkCheck check = proof.CheckRange(0, num_segments, 0, proof.num_links);
    ASSERT_TRUE(check.valid);
    ASSERT_TRUE(proof.SpotCheck().valid);
}

TEST_F(ATwistWorkProof, FailsIfTheNumberOfLinksIsLessThanTheMinimum)
{
    uint32_t minimum_number_of_links = 9;
    proof = TwistWorkProof(memory_seed, N_factor, target, link_threshold, num_segments, minimum_number_of_links);
    proof.link_threshold = proof.target;
    uint8_t keep_working = 1;

    proof.DoWork(&keep_working);
    proof.minimum_number_of_links = 10;
    ASSERT_THAT(proof.DifficultyAchieved(), Eq(0));
}

TEST_F(ATwistWorkProof, FailsIfTheTargetIsReachedBeforeTheLastLink)
{
    uint32_t minimum_number_of_links = 11;
    proof = TwistWorkProof(memory_seed, N_factor, target, link_threshold, num_segments, minimum_number_of_links);
    proof.link_threshold = proof.target;
    uint8_t keep_working = 1;

    proof.DoWork(&keep_working);
    proof.minimum_number_of_links = 10;
    TwistWorkCheck check = proof.CheckRange(0, num_segments, 0, proof.num_links);
    ASSERT_FALSE(check.valid);
    ASSERT_TRUE(check.VerifyInvalid(proof));
}