#ifndef TELEPORT_TELEPORTMINER_H
#define TELEPORT_TELEPORTMINER_H

#include <src/crypto/uint256.h>
#include <map>
#include <src/mining/work.h>
#include "NetworkMiningInfo.h"
#include "MiningHashTree.h"




class TeleportMiner
{
public:
    std::map<uint256, NetworkMiningInfo> network_id_to_mining_info;
    MiningHashTree tree;
    SimpleWorkProof proof;
    uint32_t megabytes_used;

    TeleportMiner(): megabytes_used(TELEPORT_WORK_NUMBER_OF_MEGABYTES) { }

    void AddNetworkMiningInfo(NetworkMiningInfo info);

    bool IsMining();

    std::vector<uint256> BranchForNetwork(uint256 network_id);

    uint256 MiningRoot();

    void StartMining();

    SimpleWorkProof GetProof();

    uint160 GetMaxDifficulty();

    void InformNetworksOfProofOfWork();

    void InformNetworkOfProofOfWork(NetworkMiningInfo info);
};


#endif //TELEPORT_TELEPORTMINER_H
