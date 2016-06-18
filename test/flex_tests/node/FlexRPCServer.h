#ifndef FLEX_FLEXRPCSERVER_H
#define FLEX_FLEXRPCSERVER_H


#include <jsonrpccpp/server/connectors/httpserver.h>
#include <jsonrpccpp/server/abstractserver.h>
#include <src/crypto/uint256.h>
#include "HttpAuthServer.h"


class FlexLocalServer;


class FlexRPCServer : public jsonrpc::AbstractServer<FlexRPCServer>
{
public:
    uint256 network_id;
    std::map<std::string,std::string> headers;
    std::string response{"a response"};
    FlexLocalServer *flex_local_server{NULL};

    FlexRPCServer(jsonrpc::HttpAuthServer &server) :
            jsonrpc::AbstractServer<FlexRPCServer>(server)
    {
        BindMethod("help", &FlexRPCServer::Help);
        BindMethod("getinfo", &FlexRPCServer::GetInfo);
        BindMethod("setnetworkid", &FlexRPCServer::SetNetworkID);
        BindMethod("set_megabytes_used", &FlexRPCServer::SetMegabytesUsed);
        BindMethod("new_proof", &FlexRPCServer::NewProof);
        BindMethod("balance", &FlexRPCServer::Balance);
        BindMethod("start_mining", &FlexRPCServer::StartMining);
        BindMethod("start_mining_asynchronously", &FlexRPCServer::StartMiningAsynchronously);
    }

    void SetFlexLocalServer(FlexLocalServer *flex_local_server_);

    void Help(const Json::Value& request, Json::Value& response);

    void GetInfo(const Json::Value& request, Json::Value& response);

    void SetNetworkID(const Json::Value& request, Json::Value& response);

    void SetMegabytesUsed(const Json::Value& request, Json::Value& response);

    void NewProof(const Json::Value& request, Json::Value& response);

    void Balance(const Json::Value& request, Json::Value& response);

    void StartMining(const Json::Value& request, Json::Value& response);

    void StartMiningAsynchronously(const Json::Value& request, Json::Value& response);

    void BindMethod(const char* method_name,
                    void (FlexRPCServer::*method)(const Json::Value &,Json::Value &));

};


#endif //FLEX_FLEXRPCSERVER_H
