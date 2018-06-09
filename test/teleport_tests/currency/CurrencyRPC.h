#ifndef TELEPORT_CURRENCYRPC_H
#define TELEPORT_CURRENCYRPC_H

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <test/teleport_tests/node/Data.h>
#include <test/teleport_tests/node/config/TeleportConfig.h>
#include "define.h"
#include "string_functions.h"

//
//class CurrencyRPC
//{
//public:
//    int64_t port;
//    std::string host;
//    std::string user;
//    std::string password;
//    std::string currency_code;
//    jsonrpc::HttpClient *http_client{NULL};
//    jsonrpc::Client *client{NULL};
//    TeleportConfig &config;
//
//    CurrencyRPC(std::string currency_code, TeleportConfig &config_):
//        currency_code(currency_code), config(config_)
//    {
//        port = string_to_int(CurrencyInfo("rpcport"));
//        host = CurrencyInfo("rpchost");
//
//        if (host == "")
//            host = "127.0.0.1";
//
//        user = CurrencyInfo("rpcuser");
//        password = CurrencyInfo("rpcpassword");
//
//        http_client = new jsonrpc::HttpClient("http://" + host + ":" + ToString(port));
//        http_client->AddHeader("Authorization", "Basic " + user + ":" + password);
//        client = new jsonrpc::Client(*http_client);
//    }
//
//    std::string CurrencyInfo(std::string parameter_name)
//    {
//        return config[currency_code + "-" + parameter_name];
//    }
//
//    Json::Value ExecuteCommand(std::string command)
//    {
//        std::string method;
//        std::vector<std::string> params;
//
//        if (!GetMethodAndParamsFromCommand(method, params, command))
//            return Json::Value();
//
//        //Json::Value converted_params = ConvertParameterTypes(method, params);
//        Json::Value json_params;
//
//        for (auto param : params)
//            json_params.append(param);
//
//        auto reply = client->CallMethod(method, json_params);
//        return reply;
//    }
//};
//

#endif //TELEPORT_CURRENCYRPC_H
