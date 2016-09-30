#ifndef TELEPORT_MININGHASHTREE_H
#define TELEPORT_MININGHASHTREE_H

#include "crypto/uint256.h"
#include "crypto/hashtrees.h"

class MiningHashTree
{
public:
    std::vector<uint256> hashes;
    std::vector<std::vector<uint256> > layers;
    bool up_to_date;

    MiningHashTree(): up_to_date(false) { }

    uint256 Root();

    void AddHash(uint256 hash);

    static uint256 Combine(uint256 hash1, uint256 hash2);

    static uint256 Hash(uint256 input_hash);

    std::vector<uint256> Branch(uint256 hash);

    static uint256 EvaluateBranch(std::vector<uint256> branch);

    void GenerateLayers();
};


#endif //TELEPORT_MININGHASHTREE_H
