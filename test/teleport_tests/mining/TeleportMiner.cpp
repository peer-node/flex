#include "TeleportMiner.h"
#include "NetworkSpecificProofOfWork.h"

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

using namespace jsonrpc;
using namespace std;


bool TeleportMiner::IsMining()
{
    return false;
}

void TeleportMiner::AddNetworkMiningInfo(NetworkMiningInfo info)
{
    network_id_to_mining_info[info.network_id] = info;
    tree.AddHash(info.network_seed);
}

std::vector<uint256> TeleportMiner::BranchForNetwork(uint256 network_id)
{
    uint256 network_seed = network_id_to_mining_info[network_id].network_seed;
    return tree.Branch(network_seed);
}

uint256 TeleportMiner::MiningRoot()
{
    return tree.Root();
}

uint160 TeleportMiner::GetMaxDifficulty()
{
    uint160 max_difficulty = 0;
    for (auto entry : network_id_to_mining_info)
    {
        if (entry.second.difficulty > max_difficulty)
            max_difficulty = entry.second.difficulty;
    }
    return max_difficulty;
}

void TeleportMiner::StartMining()
{
    auto seed = MiningRoot();
    uint160 difficulty = GetMaxDifficulty();
    proof = SimpleWorkProof(seed, difficulty);
    uint8_t working = 1;
    proof.DoWork(&working);
    InformNetworksOfProofOfWork();
}

void TeleportMiner::InformNetworksOfProofOfWork()
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

void TeleportMiner::InformNetworkOfProofOfWork(NetworkMiningInfo info)
{
    NetworkSpecificProofOfWork network_proof(BranchForNetwork(info.network_id), proof);

    Json::Value params;
    params["proof_base64"] = network_proof.GetBase64String();

    string url = info.NotificationUrl();
    HttpClient http_client(url);
    Client client(http_client);

    try
    {
        client.CallMethod("new_proof", params);
    }
    catch (JsonRpcException e)
    {
        std::cerr << "failed to send proof to server: " << e.GetMessage() << "\n";
        std::cerr << "trying again in 5 seconds\n";
        sleep(5);
        client.CallMethod("new_proof", params);
    }


}

SimpleWorkProof TeleportMiner::GetProof()
{
    return proof;
}
