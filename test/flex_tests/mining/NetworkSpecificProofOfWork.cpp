#include "NetworkSpecificProofOfWork.h"
#include "base/serialize.h"
#include "base/base64.h"
#include "MiningHashTree.h"


uint64_t MemoryFactorFromNumberOfMegabytes(uint64_t number_of_megabytes)
{
    uint64_t memory_factor = 7;
    while (number_of_megabytes > 1)
    {
        memory_factor += 1;
        number_of_megabytes /= 2;
    }
    return memory_factor;
}

NetworkSpecificProofOfWork::NetworkSpecificProofOfWork(std::vector<uint256> branch, TwistWorkProof proof):
        branch(branch),
        proof(proof)
{

}

std::string NetworkSpecificProofOfWork::GetBase64String()
{
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << *this;
    std::string serialization = ss.str();
    std::string base64_string = EncodeBase64(serialization);
    return base64_string;
}

NetworkSpecificProofOfWork::NetworkSpecificProofOfWork(std::string base64_string)
{
    std::string decoded = DecodeBase64(base64_string);
    vch_t data(decoded.begin(), decoded.end());
    CDataStream ss(data, SER_NETWORK, CLIENT_VERSION);
    ss >> *this;
}

bool NetworkSpecificProofOfWork::IsValid()
{
    return MiningHashTree::EvaluateBranch(branch) == proof.memoryseed and proof.DifficultyAchieved() != 0;
}
