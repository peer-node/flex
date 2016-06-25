#include <jsonrpccpp/client.h>

#define private public // to allow access to HttpClient.curl
#include <jsonrpccpp/client/connectors/httpclient.h>
#undef private

#include <fstream>
#include <test/flex_tests/mining/MiningRPCServer.h>
#include "gmock/gmock.h"
#include "FlexLocalServer.h"
#include "FlexNetworkNode.h"


using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class AFlexLocalServer : public Test
{
public:
    MemoryDataStore creditdata, msgdata;
    FlexLocalServer flex_local_server;
    FlexNetworkNode flex_network_node;
    string config_filename;
    int argc = 2;
    const char *argv[2] = {"", "-conf=flex_tmp_config_k5i4jn5u.conf"};

    virtual void SetUp()
    {
        flex_network_node.credit_system->initial_difficulty = 10000;
        flex_local_server.SetNetworkNode(&flex_network_node);
        config_filename = flex_local_server.config_parser.GetDataDir().string() + "/flex_tmp_config_k5i4jn5u.conf";
    }

    virtual void WriteConfigFile(string config)
    {
        ofstream stream(config_filename);
        stream << config << endl;
        stream.close();
        chmod(config_filename.c_str(), S_IRUSR | S_IWUSR);
    }

    virtual void DeleteConfigFile()
    {
        remove(config_filename.c_str());
    }

    virtual void TearDown()
    {
        DeleteConfigFile();
    }
};

TEST_F(AFlexLocalServer, ReadsCommandLineArgumentsAndConfig)
{
    WriteConfigFile("a=b");
    flex_local_server.LoadConfig(argc, argv);
    ASSERT_THAT(flex_local_server.config["a"], Eq("b"));
}

TEST_F(AFlexLocalServer, ThrowsAnExceptionIfNoRPCUsernameOrPasswordAreInTheConfigFile)
{
    WriteConfigFile("a=b");
    flex_local_server.LoadConfig(argc, argv);
    ASSERT_THROW(flex_local_server.StartRPCServer(), std::runtime_error);
}

TEST_F(AFlexLocalServer, ProvidesAMiningSeed)
{
    uint256 mining_seed = flex_local_server.MiningSeed();
}

class AFlexLocalServerWithAMiningRPCServer : public AFlexLocalServer
{
public:
    HttpAuthServer *mining_http_server;
    MiningRPCServer *mining_rpc_server;
    HttpClient *mining_http_client;
    Client *mining_client;

    virtual void SetUp()
    {
        AFlexLocalServer::SetUp();
        mining_http_server = new HttpAuthServer(8339, "username", "password");
        mining_rpc_server = new MiningRPCServer(*mining_http_server);
        mining_rpc_server->StartListening();
        mining_http_client = new HttpClient("http://localhost:8339");
        mining_http_client->AddHeader("Authorization", "Basic username:password");
        mining_client = new Client(*mining_http_client);
        WriteConfigFile("miningrpcport=8339\n"
                        "miningrpcuser=username\n"
                        "miningrpcpassword=password\n"
                        "rpcuser=flexuser\n"
                        "rpcpassword=abcd123\n"
                        "rpcport=8383\n");
        flex_local_server.LoadConfig(argc, argv);
    }

    virtual void TearDown()
    {
        AFlexLocalServer::TearDown();
        mining_rpc_server->StopListening();
        delete mining_rpc_server;
        delete mining_http_server;
        delete mining_client;
        delete mining_http_client;
    }
};

TEST_F(AFlexLocalServerWithAMiningRPCServer, SetsTheNumberOfMegabytesUsed)
{
    flex_local_server.SetNumberOfMegabytesUsedForMining(1);
    auto result = mining_client->CallMethod("get_megabytes_used", Json::Value());
    ASSERT_THAT(result.asInt64(), Eq(1));
}

TEST_F(AFlexLocalServerWithAMiningRPCServer, SetsTheNetworkInfo)
{
    flex_local_server.SetMiningNetworkInfo();
    auto seed_list = mining_client->CallMethod("list_seeds", Json::Value());
    ASSERT_THAT(seed_list.size(), Eq(1));
}

TEST_F(AFlexLocalServerWithAMiningRPCServer, ReceivesTheNewProofNotificationFromTheMiner)
{
    flex_local_server.SetNumberOfMegabytesUsedForMining(1);
    flex_local_server.SetMiningNetworkInfo();
    flex_local_server.StartRPCServer();
    mining_client->CallMethod("start_mining", Json::Value());
    flex_local_server.StopRPCServer();
    NetworkSpecificProofOfWork proof = flex_local_server.GetLatestProofOfWork();
    ASSERT_THAT(proof.MegabytesUsed(), Eq(1));
    ASSERT_TRUE(proof.IsValid());
}


class AFlexLocalServerWithRPCSettings : public AFlexLocalServer
{
public:
    HttpClient *http_client;
    Client *client;

    virtual void SetUp()
    {
        AFlexLocalServer::SetUp();
        WriteConfigFile("rpcuser=flexuser\n"
                        "rpcpassword=abcd123\n"
                        "rpcport=8383\n");
        flex_local_server.LoadConfig(argc, argv);
        flex_local_server.StartRPCServer();

        http_client = new HttpClient("http://localhost:8383");
        http_client->AddHeader("Authorization", "Basic flexuser:abcd123");
        client = new Client(*http_client);
    }

    virtual void TearDown()
    {
        AFlexLocalServer::TearDown();
        delete client;
        delete http_client;
        flex_local_server.StopRPCServer();
    }
};

TEST_F(AFlexLocalServerWithRPCSettings, StartsAnRPCServer)
{
    Json::Value params;

    auto result = client->CallMethod("help", params);
    ASSERT_THAT(result.asString(), Ne(""));
}

TEST_F(AFlexLocalServerWithRPCSettings, ProvidesABalance)
{
    Json::Value params;

    auto result = client->CallMethod("balance", params);
    ASSERT_THAT(result.asString(), Eq("0.00"));
}


class AFlexLocalServerWithAMiningRPCServerAndAFlexNetworkNode : public AFlexLocalServerWithAMiningRPCServer
{
public:
    HttpClient *http_client;
    Client *client;

    virtual void SetUp()
    {
        AFlexLocalServerWithAMiningRPCServer::SetUp();
        flex_local_server.SetNumberOfMegabytesUsedForMining(1);
        flex_local_server.SetMiningNetworkInfo();
        flex_local_server.StartRPCServer();

        http_client = new HttpClient("http://localhost:8383");
        http_client->AddHeader("Authorization", "Basic flexuser:abcd123");
        client = new Client(*http_client);
    }

    virtual void TearDown()
    {
        AFlexLocalServerWithAMiningRPCServer::TearDown();
        delete client;
        delete http_client;
        flex_local_server.StopRPCServer();
    }
};

TEST_F(AFlexLocalServerWithAMiningRPCServerAndAFlexNetworkNode, StartsMining)
{
    client->CallMethod("start_mining", Json::Value());
    NetworkSpecificProofOfWork proof = flex_local_server.GetLatestProofOfWork();
    ASSERT_THAT(proof.MegabytesUsed(), Eq(1));
    ASSERT_TRUE(proof.IsValid());
}

TEST_F(AFlexLocalServerWithAMiningRPCServerAndAFlexNetworkNode, AddsAMinedCreditMessageToTheTipWhenMined)
{
    client->CallMethod("start_mining", Json::Value());
    NetworkSpecificProofOfWork proof = flex_local_server.GetLatestProofOfWork();
    MinedCreditMessage tip = flex_local_server.flex_network_node->Tip();
    ASSERT_THAT(tip.proof_of_work, Eq(proof));
    ASSERT_THAT(proof.branch[0], Eq(tip.mined_credit.GetHash()));
}

