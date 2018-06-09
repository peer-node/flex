#include <src/crypto/point.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "gmock/gmock.h"

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <test/teleport_tests/node/server/HttpAuthServer.h>
#include <test/teleport_tests/currency/Currency.h>

using namespace ::testing;
using namespace std;
using namespace jsonrpc;

#include "log.h"
#define LOG_CATEGORY "test"
//
//
//class TestCurrencyServer : public AbstractServer<TestCurrencyServer>
//{
//public:
//    vector<string> methods;
//
//    TestCurrencyServer(HttpAuthServer &server) : AbstractServer<TestCurrencyServer>(server)
//    {
//        BindMethod("help", &TestCurrencyServer::Help);
//        BindMethod("testMethod", &TestCurrencyServer::TestMethod);
//        BindMethod("getbalance", &TestCurrencyServer::GetBalance);
//        BindMethod("dumpprivkey", &TestCurrencyServer::DumpPrivKey);
//        BindMethod("getnewaddress", &TestCurrencyServer::GetNewAddress);
//        BindMethod("getinfo", &TestCurrencyServer::GetInfo);
//        BindMethod("listunspent", &TestCurrencyServer::ListUnspent);
//        BindMethod("sendtoaddress", &TestCurrencyServer::SendToAddress);
//        BindMethod("importprivkey", &TestCurrencyServer::ImportPrivKey);
//        BindMethod("gettxout", &TestCurrencyServer::GetTxOut);
//    }
//
//    void BindMethod(const char* method_name, void (TestCurrencyServer::*method)(const Json::Value &,Json::Value &))
//    {
//        this->bindAndAddMethod(Procedure(method_name, PARAMS_BY_POSITION, JSON_STRING, NULL), method);
//        methods.push_back(string(method_name));
//    }
//
//    void Help(const Json::Value& request, Json::Value& response)
//    {
//        for (auto method : methods)
//            response.append(method);
//    }
//
//    void TestMethod(const Json::Value& request, Json::Value& response)
//    {
//        response = request;
//    }
//
//    void GetBalance(const Json::Value& request, Json::Value& response)
//    {
//        response = 100.0;
//    }
//
//    void DumpPrivKey(const Json::Value& request, Json::Value& response)
//    {
//        response = "5JG3U9EHwnGpTadiFEL2EEu3ZTtPbwqNke8eaTDnWWCZC7mMDdy";
//    }
//
//    void GetNewAddress(const Json::Value& request, Json::Value& response)
//    {
//        response = "1JWdjTyGeQsAPcRyr4dvuqGDFxA1QkeTGf";
//    }
//
//    void ImportPrivKey(const Json::Value& request, Json::Value& response)
//    {
//        response = "1JWdjTyGeQsAPcRyr4dvuqGDFxA1QkeTGf";
//    }
//
//    void GetInfo(const Json::Value& request, Json::Value& response)
//    {
//        response["unlocked_until"] = 1900000000;
//    }
//
//    void ListUnspent(const Json::Value& request, Json::Value& response)
//    {
//        response.append(ATxOut());
//    }
//
//    void SendToAddress(const Json::Value& request, Json::Value& response)
//    {
//        response = "5535c483e7776";
//    }
//
//    void GetTxOut(const Json::Value& request, Json::Value& response)
//    {
//        response = ATxOut();
//    }
//
//    Json::Value ATxOut()
//    {
//        Json::Value unspent;
//        Json::Value scriptPubKey;
//        Json::Value addresses;
//        addresses.append("13ixW1VDZt8f6L5yuuCfYTeoDxbGRYY6AY");
//        scriptPubKey["addresses"] = addresses;
//        scriptPubKey["type"] = "pubkeyhash";
//        unspent["scriptPubKey"] = scriptPubKey;
//        unspent["vout"] = 0;
//        unspent["value"] = 10000;
//        unspent["n"] = 0;
//        unspent["amount"] = 10000;
//        unspent["confirmations"] = 30;
//        return unspent;
//    }
//};
//
//class ATestCurrencyServer : public Test
//{
//public:
//    HttpAuthServer *http_server;
//    TestCurrencyServer *server;
//    TeleportConfig config;
//
//    virtual void SetUp()
//    {
//        http_server = new HttpAuthServer(8383, "username", "password");
//        server = new TestCurrencyServer(*http_server);
//        server->StartListening();
//
//        config["TEST-rpcport"] = "8383";
//        config["TEST-rpcuser"] = "username";
//        config["TEST-rpcpassword"] = "password";
//    }
//
//    virtual void TearDown()
//    {
//        server->StopListening();
//        delete server;
//        delete http_server;
//    }
//};
//
//TEST_F(ATestCurrencyServer, ReceivesRPCRequests)
//{
//    CurrencyRPC rpc("TEST", config);
//
//    auto response = rpc.ExecuteCommand("testMethod hello");
//
//    ASSERT_THAT(response[0].asString(), Eq("hello"));
//}
//
//
//class ACurrency : public ATestCurrencyServer
//{
//public:
//    Currency *currency;
//
//    virtual void SetUp()
//    {
//        ATestCurrencyServer::SetUp();
//        currency = new Currency("TEST", config);
//    }
//
//    virtual void TearDown()
//    {
//        delete currency;
//        ATestCurrencyServer::TearDown();
//    }
//};
//
//TEST_F(ACurrency, Initializes)
//{
//    currency->Initialize();
//    ASSERT_THAT(currency->is_cryptocurrency, Eq(true));
//}
//
//
//class AnInitializedCurrency : public ACurrency
//{
//public:
//    virtual void SetUp()
//    {
//        ACurrency::SetUp();
//        currency->Initialize();
//    }
//
//    virtual void TearDown()
//    {
//        ACurrency::TearDown();
//    }
//};
//
//TEST_F(AnInitializedCurrency, RequestsHelp)
//{
//    auto result = currency->crypto->Help();
//    log_ << result << "\n";
//}
//
//TEST_F(AnInitializedCurrency, IsUsable)
//{
//    ASSERT_TRUE(currency->crypto->usable);
//}
//
//
