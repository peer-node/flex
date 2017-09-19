#ifndef TELEPORT_TESTELECTRUMSERVER_H
#define TELEPORT_TESTELECTRUMSERVER_H

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <test/teleport_tests/node/server/HttpAuthServer.h>
#include <src/crypto/bignum.h>

#include "log.h"
#define LOG_CATEGORY "TestElectrumServer"


class TestElectrumServer : public jsonrpc::AbstractServer<TestElectrumServer>
{
public:
    std::vector<std::string> methods;
    std::vector<Json::Value> unspent_txouts;
    std::vector<std::string> addresses;
    std::vector<CBigNum> private_keys;

    TestElectrumServer(jsonrpc::HttpServer &server) : jsonrpc::AbstractServer<TestElectrumServer>(server)
    {
        BindMethod("help", &TestElectrumServer::Help);
        BindMethod("broadcast", &TestElectrumServer::Broadcast);
        BindMethod("getbalance", &TestElectrumServer::GetBalance);
        BindMethod("getprivatekeys", &TestElectrumServer::GetPrivateKeys);
        BindMethod("getunusedaddress", &TestElectrumServer::GetUnusedAddress);
        BindMethod("listunspent", &TestElectrumServer::ListUnspent);
        BindMethod("payto", &TestElectrumServer::SendToAddress);
        BindMethod("importprivkey", &TestElectrumServer::ImportPrivKey);
        BindMethod("version", &TestElectrumServer::Version);
    }

    void BindMethod(const char *method_name, void (TestElectrumServer::*method)(const Json::Value &, Json::Value &))
    {
        this->bindAndAddMethod(jsonrpc::Procedure(method_name, jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL), method);
        methods.push_back(std::string(method_name));
    }

    void Help(const Json::Value& request, Json::Value& response)
    {
        for (auto method : methods)
            response.append(method);
    }
};


#endif //TELEPORT_TESTELECTRUMSERVER_H
