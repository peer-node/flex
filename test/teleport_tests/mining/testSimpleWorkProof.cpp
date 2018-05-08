#include "gmock/gmock.h"
#include "mining/work.h"

using namespace ::testing;

class ASimpleWorkProof : public Test
{
public:
    SimpleWorkProof proof;
    uint256 seed;
    uint160 difficulty;

    virtual void SetUp()
    {
        seed = 123456789;
        difficulty = 10000;
        proof = SimpleWorkProof(seed, difficulty);
    }
};

TEST_F(ASimpleWorkProof, DoesTheWork)
{
    uint8_t keep_working = 1;
    proof.DoWork(&keep_working);
    ASSERT_THAT(proof.WorkDone(), Gt(0));
}

TEST_F(ASimpleWorkProof, PassesFullVerification)
{
    uint8_t keep_working = 1;
    proof.DoWork(&keep_working);
    ASSERT_TRUE(proof.IsValidProofOfWork());
}
