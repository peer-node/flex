#ifndef FLEX_FLEXNODE_H
#define FLEX_FLEXNODE_H

#include "ConfigParser.h"
#include "FlexRPCServer.h"
#include <jsonrpccpp/server/connectors/httpserver.h>

class FlexNode
{
public:
    ConfigParser config_parser;
    FlexConfig config;
    FlexRPCServer *rpc_server;
    jsonrpc::HttpAuthServer *http_server;

    void StartRPCServer();

    void ThrowUsernamePasswordException();

    void StopRPCServer();

    void LoadConfig(int argc, const char *argv[]);
};


#endif //FLEX_FLEXNODE_H
