#include "MiningHashTree.h"

#define NO_POSITION -1

using std::vector;

uint160 u256tou160(uint256 n)
{
    uint160 result;
    memcpy(result.begin(), n.begin(), 20);
    return result;
}

vector<uint256> GenerateNextLayerOfHashes(vector<uint256> previous_layer);

uint256 MiningHashTree::Root()
{
    if (hashes.size() == 0)
        return 0;

    if (not up_to_date)
        GenerateLayers();

    return layers.back()[0];
}

void MiningHashTree::GenerateLayers()
{
    layers.resize(0);
    layers.push_back(GenerateNextLayerOfHashes(hashes));

    while (layers.back().size() != 1)
        layers.push_back(GenerateNextLayerOfHashes(layers.back()));

    up_to_date = true;
}

vector<uint256> GenerateNextLayerOfHashes(vector<uint256> previous_layer)
{
    vector<uint256> next_layer;

    for (uint32_t i = 0; i < previous_layer.size(); i += 2)
    {
        if (i + 2 <= previous_layer.size())
            next_layer.push_back(MiningHashTree::Combine(previous_layer[i], previous_layer[i + 1]));
        else
            next_layer.push_back(previous_layer[i]);
    }

    return next_layer;
}

void MiningHashTree::AddHash(uint256 hash)
{
    hashes.push_back(hash);
    up_to_date = false;
}

uint256 MiningHashTree::Hash(uint256 input_hash)
{
    return ::Hash(BEGIN(input_hash), END(input_hash));
}

uint256 MiningHashTree::Combine(uint256 hash1, uint256 hash2)
{
    uint8_t data[64];
    uint256 *hash1_xor_hash2 = (uint256*)&data[0];
    *hash1_xor_hash2 = hash1 ^ hash2;

    uint256 *hash_of_hash1_xor_hash_of_hash2 = (uint256*)&data[32];
    *hash_of_hash1_xor_hash_of_hash2 = Hash(hash1) ^ Hash(hash2);

    return ::Hash(BEGIN(data), END(data));
}

uint64_t PositionOfHashInVector(uint256 hash, vector<uint256> hashes)
{
    return std::find(hashes.begin(), hashes.end(), hash) - hashes.begin();
}

vector<int64_t> BranchElementPositions(uint64_t hash_position, uint64_t number_of_hashes)
{
    vector<int64_t> positions;
    int64_t position = hash_position;

    for (; number_of_hashes > 1; number_of_hashes = (number_of_hashes + 1) / 2)
    {
        if (position % 2 == 0)
        {
            if (position == number_of_hashes - 1)
                positions.push_back(NO_POSITION);
            else
                positions.push_back(position + 1);
        }
        else
            positions.push_back(position - 1);

        position = position / 2;
    }
    return positions;
}

vector<uint256> MiningHashTree::Branch(uint256 hash)
{
    vector<uint256> branch{hash};

    uint64_t position = PositionOfHashInVector(hash, hashes);
    vector<int64_t> branch_element_positions = BranchElementPositions(position, hashes.size());

    if (hashes.size() > 1 and branch_element_positions[0] != NO_POSITION)
        branch.push_back(hashes[branch_element_positions[0]]);

    if (not up_to_date)
        GenerateLayers();

    for (uint32_t i = 1 ; i < layers.size(); i++)
        if (branch_element_positions[i] != NO_POSITION)
            branch.push_back(layers[i - 1][branch_element_positions[i]]);

    return branch;
}

uint256 MiningHashTree::EvaluateBranch(vector<uint256> branch)
{
    if (branch.size() == 0)
        return 0;
    auto hash = branch[0];
    for (uint32_t i = 1; i < branch.size(); i++)
        hash = Combine(hash, branch[i]);
    return hash;
}


#undef NO_POSITION
