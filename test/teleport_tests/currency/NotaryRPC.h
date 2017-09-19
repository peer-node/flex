#ifndef TELEPORT_NOTARYRPC_H
#define TELEPORT_NOTARYRPC_H

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <test/teleport_tests/node/Data.h>
#include <test/teleport_tests/node/config/TeleportConfig.h>
#include "define.h"
#include "string_functions.h"


class NotaryRPC
{
public:
    int64_t port;
    std::string host;
    std::string currency_code;
    jsonrpc::HttpClient *http_client{NULL};
    jsonrpc::Client *client{NULL};
    TeleportConfig &config;

    NotaryRPC(std::string currency_code, TeleportConfig &config):
        currency_code(currency_code), config(config)
    {
        port = string_to_int(Info("notaryrpcport"));
        host = Info("notaryrpchost");
        if (host == "")
            host = "127.0.0.1";

        http_client = new jsonrpc::HttpClient("http://" + host + ":" + ToString(port));
        client = new jsonrpc::Client(*http_client);
    }

    std::string Info(std::string key)
    {
        return config[currency_code + "-" + key];
    }

    Json::Value ExecuteCommand(std::string command)
    {
        std::string method;
        std::vector<std::string> params;

        if (!GetMethodAndParamsFromCommand(method, params, command))
            return Json::Value();

        // Json::Value converted_params = ConvertParameterTypes(method, params);

        Json::Value json_params;

        for (auto param : params)
            json_params.append(param);

        auto reply = client->CallMethod(method, json_params);
        return reply;
    }

};


#endif //TELEPORT_NOTARYRPC_H
