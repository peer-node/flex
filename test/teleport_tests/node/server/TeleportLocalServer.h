#ifndef TELEPORT_TELEPORTLOCALSERVER_H
#define TELEPORT_TELEPORTLOCALSERVER_H

#include "test/teleport_tests/node/config/ConfigParser.h"
#include "TeleportRPCServer.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"
#include <jsonrpccpp/server/connectors/httpserver.h>
#include <test/teleport_tests/mining/NetworkSpecificProofOfWork.h>


class TeleportLocalServer
{
public:
    ConfigParser config_parser;
    TeleportConfig config;
    TeleportRPCServer *rpc_server{NULL};
    jsonrpc::HttpAuthServer *http_server{NULL};
    TeleportNetworkNode *teleport_network_node{NULL};
    NetworkSpecificProofOfWork latest_proof_of_work;


    bool StartRPCServer();

    void ThrowUsernamePasswordException();

    void StopRPCServer();

    void LoadConfig(int argc, const char *argv[]);

    uint256 MiningSeed();

    void SetMiningNetworkInfo();

    void SetNumberOfMegabytesUsedForMining(uint32_t number_of_megabytes);

    Json::Value MakeRequestToMiningRPCServer(std::string method, Json::Value &request);

    std::string MiningAuthorization();

    uint256 NetworkID();

    uint64_t RPCPort();

    std::string ExternalIPAddress();

    NetworkSpecificProofOfWork GetLatestProofOfWork();

    void SetNetworkNode(TeleportNetworkNode *teleport_network_node_);

    uint64_t Balance();

    void StartMining();

    void HandleNewProof(NetworkSpecificProofOfWork proof);

    uint160 MiningDifficulty(uint256 mining_seed);

    void StartMiningAsynchronously();

    uint160 SendToPublicKey(Point public_key, int64_t amount);
};


#endif //TELEPORT_TELEPORTLOCALSERVER_H
