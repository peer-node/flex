#ifndef FLEX_FLEXLOCALSERVER_H
#define FLEX_FLEXLOCALSERVER_H

#include "ConfigParser.h"
#include "FlexRPCServer.h"
#include "FlexNetworkNode.h"
#include <jsonrpccpp/server/connectors/httpserver.h>
#include <test/flex_tests/mining/NetworkSpecificProofOfWork.h>


class FlexLocalServer
{
public:
    ConfigParser config_parser;
    FlexConfig config;
    FlexRPCServer *rpc_server{NULL};
    jsonrpc::HttpAuthServer *http_server{NULL};
    FlexNetworkNode *flex_network_node{NULL};
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

    void SetNetworkNode(FlexNetworkNode *flex_network_node_);

    double Balance();

    void StartMining();

    void HandleNewProof(NetworkSpecificProofOfWork proof);

    uint160 MiningDifficulty(uint256 mining_seed);

    void StartMiningAsynchronously();

    uint160 SendToPublicKey(Point public_key, int64_t amount);
};


#endif //FLEX_FLEXLOCALSERVER_H
