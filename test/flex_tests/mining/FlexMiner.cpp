#include "FlexMiner.h"
#include "mining/work.h"

bool FlexMiner::IsMining()
{
    return false;
}

void FlexMiner::AddNetworkMiningInfo(NetworkMiningInfo info)
{
    network_id_to_mining_info[info.network_id] = info;
    tree.AddHash(info.network_seed);
}

void FlexMiner::AddRPCConnection(RPCConnection *connection)
{
    network_id_to_network_connection[connection->network_id] = connection;
}

std::vector<uint256> FlexMiner::BranchForNetwork(uint256 network_id)
{
    uint256 network_seed = network_id_to_mining_info[network_id].network_seed;
    return tree.Branch(network_seed);
}

uint256 FlexMiner::MiningRoot()
{
    return tree.Root();
}

uint160 FlexMiner::GetMaxDifficulty()
{
    uint160 max_difficulty = 0;
    for (auto entry : network_id_to_mining_info)
    {
        if (entry.second.difficulty > max_difficulty)
            max_difficulty = entry.second.difficulty;
    }
    return max_difficulty;
}

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

void FlexMiner::StartMining()
{
    uint256 memory_seed = MiningRoot();
    uint160 difficulty = GetMaxDifficulty();
    uint64_t memory_factor = MemoryFactorFromNumberOfMegabytes(megabytes_used);
    proof = TwistWorkProof(memory_seed, memory_factor, difficulty);
    uint8_t working = 1;
    proof.DoWork(&working);
}

TwistWorkProof FlexMiner::GetProof()
{
    return proof;
}

void FlexMiner::SetMemoryUsageInMegabytes(uint32_t number_of_megabytes)
{
    megabytes_used = number_of_megabytes;
}
