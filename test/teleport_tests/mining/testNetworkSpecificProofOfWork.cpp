#include <src/crypto/uint256.h>
#include <src/mining/work.h>
#include "gmock/gmock.h"
#include "NetworkSpecificProofOfWork.h"

using namespace ::testing;


class ANetworkSpecificProofOfWork : public Test
{
public:
    NetworkSpecificProofOfWork proof;
};

TEST_F(ANetworkSpecificProofOfWork, HasNoBranchInitially)
{
    std::vector<uint256> branch = proof.branch;
    ASSERT_THAT(branch.size(), Eq(0));
}

TEST_F(ANetworkSpecificProofOfWork, HasNoProofInitially)
{
    SimpleWorkProof simple_work_proof = proof.proof;
    ASSERT_THAT(simple_work_proof.IsValidProofOfWork(), Eq(false));
}

class ANetworkSpecificProofOfWorkWithData : public ANetworkSpecificProofOfWork
{
public:
    virtual void SetUp()
    {
        std::vector<uint256> branch{1};
        SimpleWorkProof simple_work_proof(branch[0], 100);
        proof = NetworkSpecificProofOfWork(branch, simple_work_proof);
    }
};

TEST_F(ANetworkSpecificProofOfWorkWithData, RetainsItsData)
{
    ASSERT_THAT(proof.branch[0], Eq(1));
    ASSERT_THAT(proof.proof.seed, Eq(1));
}

TEST_F(ANetworkSpecificProofOfWorkWithData, CanBeSerializedIntoABase64EncodedString)
{
    std::string base64_string = proof.GetBase64String();
    NetworkSpecificProofOfWork other_proof(base64_string);
    ASSERT_THAT(proof, Eq(other_proof));
}

TEST_F(ANetworkSpecificProofOfWorkWithData, CanBeValidated)
{
    ASSERT_FALSE(proof.IsValid());
    uint8_t working = 1;
    proof.proof.DoWork(&working);
    ASSERT_TRUE(proof.IsValid());
}