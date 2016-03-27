#ifndef FLEX_FLEXRPCSERVER_H
#define FLEX_FLEXRPCSERVER_H


#include <jsonrpccpp/server/connectors/httpserver.h>
#include <jsonrpccpp/server/abstractserver.h>
#include <src/crypto/uint256.h>
#include "HttpAuthServer.h"


class FlexRPCServer : public jsonrpc::AbstractServer<FlexRPCServer>
{
public:
    uint256 network_id;
    std::map<std::string,std::string> headers;
    std::string response{"a response"};

    FlexRPCServer(jsonrpc::HttpAuthServer &server) :
            jsonrpc::AbstractServer<FlexRPCServer>(server)
    {
        BindMethod("help", &FlexRPCServer::Help);
        BindMethod("getinfo", &FlexRPCServer::GetInfo);
        BindMethod("setnetworkid", &FlexRPCServer::SetNetworkID);

    }

    void Help(const Json::Value& request, Json::Value& response);

    void GetInfo(const Json::Value& request, Json::Value& response);

    void SetNetworkID(const Json::Value& request, Json::Value& response);

    void BindMethod(const char* method_name,
                    void (FlexRPCServer::*method)(const Json::Value &,Json::Value &));

};


#endif //FLEX_FLEXRPCSERVER_H
