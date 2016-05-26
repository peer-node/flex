#include "FlexMiner.h"
#include "NetworkSpecificProofOfWork.h"

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

using namespace jsonrpc;
using namespace std;


bool FlexMiner::IsMining()
{
    return false;
}

void FlexMiner::AddNetworkMiningInfo(NetworkMiningInfo info)
{
    network_id_to_mining_info[info.network_id] = info;
    tree.AddHash(info.network_seed);
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

void FlexMiner::StartMining()
{
    uint256 memory_seed = MiningRoot();
    uint160 difficulty = GetMaxDifficulty();
    uint64_t memory_factor = MemoryFactorFromNumberOfMegabytes(megabytes_used);
    proof = TwistWorkProof(memory_seed, memory_factor, difficulty);
    uint8_t working = 1;
    proof.DoWork(&working);
    InformNetworksOfProofOfWork();
}

void FlexMiner::InformNetworksOfProofOfWork()
{
    Json::Value params;

    for (auto entry : network_id_to_mining_info)
    {
        NetworkMiningInfo info = entry.second;
        if (info.network_port == 0)
            continue;
        InformNetworkOfProofOfWork(info);
    }
}

void FlexMiner::InformNetworkOfProofOfWork(NetworkMiningInfo info)
{
    NetworkSpecificProofOfWork network_proof(BranchForNetwork(info.network_id), proof);

    Json::Value params;
    params["proof_base64"] = network_proof.GetBase64String();

    string url = info.NotificationUrl();
    HttpClient http_client(url);
    Client client(http_client);

    client.CallMethod("new_proof", params);
}

TwistWorkProof FlexMiner::GetProof()
{
    return proof;
}

void FlexMiner::SetMemoryUsageInMegabytes(uint32_t number_of_megabytes)
{
    megabytes_used = number_of_megabytes;
}
