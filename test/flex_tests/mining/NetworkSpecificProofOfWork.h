#ifndef FLEX_NETWORKSPECIFICPROOFOFWORK_H
#define FLEX_NETWORKSPECIFICPROOFOFWORK_H


#include <src/crypto/uint256.h>
#include <src/mining/work.h>

uint64_t MemoryFactorFromNumberOfMegabytes(uint64_t number_of_megabytes);


class NetworkSpecificProofOfWork
{
public:
    std::vector<uint256> branch;
    TwistWorkProof proof;

    NetworkSpecificProofOfWork() { }
    NetworkSpecificProofOfWork(std::vector<uint256> branch, TwistWorkProof proof);

    NetworkSpecificProofOfWork(std::string base64_string);

    std::string GetBase64String();

    bool operator==(const NetworkSpecificProofOfWork& other) const
    {
        return branch == other.branch && proof.GetHash() == other.proof.GetHash();
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(branch);
        READWRITE(proof);
    )
    
    JSON(branch, proof);

    bool IsValid();

    uint64_t MegabytesUsed()
    {
        return proof.MegabytesUsed();
    }
};


#endif //FLEX_NETWORKSPECIFICPROOFOFWORK_H
