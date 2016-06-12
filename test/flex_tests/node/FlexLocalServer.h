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
    FlexRPCServer *rpc_server;
    jsonrpc::HttpAuthServer *http_server;
    FlexNetworkNode *flex_network_node;

    void StartRPCServer();

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

    uint160 MiningDifficulty();

    std::string ExternalIPAddress();

    NetworkSpecificProofOfWork GetLatestProofOfWork();

    NetworkSpecificProofOfWork latest_proof_of_work;

    void SetNetworkNode(FlexNetworkNode *flex_network_node_);

    double Balance();
};


#endif //FLEX_FLEXLOCALSERVER_H
