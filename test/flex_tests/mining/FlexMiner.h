#ifndef FLEX_FLEXMINER_H
#define FLEX_FLEXMINER_H

#include <src/crypto/uint256.h>
#include <map>
#include <src/mining/work.h>
#include "NetworkMiningInfo.h"
#include "RPCConnection.h"
#include "MiningHashTree.h"

#define FLEX_WORK_NUMBER_OF_MEGABYTES 4096



class FlexMiner
{
public:
    std::map<uint256, NetworkMiningInfo> network_id_to_mining_info;
    std::map<uint256, RPCConnection*> network_id_to_network_connection;
    MiningHashTree tree;
    TwistWorkProof proof;
    uint32_t megabytes_used;

    FlexMiner(): megabytes_used(FLEX_WORK_NUMBER_OF_MEGABYTES) { }

    void AddNetworkMiningInfo(NetworkMiningInfo info);

    void AddRPCConnection(RPCConnection *connection);

    bool IsMining();

    std::vector<uint256> BranchForNetwork(uint256 network_id);

    uint256 MiningRoot();

    void StartMining();

    TwistWorkProof GetProof();

    uint160 GetMaxDifficulty();

    void SetMemoryUsageInMegabytes(uint32_t number_of_megabytes);
};


#endif //FLEX_FLEXMINER_H
