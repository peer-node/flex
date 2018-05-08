#include <jsonrpccpp/client.h>

#define private public // to allow access to HttpClient.curl
#include <jsonrpccpp/client/connectors/httpclient.h>
#undef private

#include <fstream>
#include <test/teleport_tests/mining/MiningRPCServer.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/server/TeleportLocalServer.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"


using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class ATeleportLocalServer : public Test
{
public:
    MemoryDataStore creditdata, msgdata;
    TeleportLocalServer teleport_local_server;
    TeleportNetworkNode teleport_network_node;
    string config_filename;
    int argc = 2;
    const char *argv[2] = {"", "-conf=teleport_tmp_config_k5i4jn5u.conf"};

    virtual void SetUp()
    {
        teleport_network_node.credit_system->initial_difficulty = 10000;
        teleport_local_server.SetNetworkNode(&teleport_network_node);
        config_filename = teleport_local_server.config_parser.GetDataDir().string() + "/teleport_tmp_config_k5i4jn5u.conf";
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

TEST_F(ATeleportLocalServer, ReadsCommandLineArgumentsAndConfig)
{
    WriteConfigFile("a=b");
    teleport_local_server.LoadConfig(argc, argv);
    ASSERT_THAT(teleport_local_server.config["a"], Eq("b"));
}

TEST_F(ATeleportLocalServer, ThrowsAnExceptionIfNoRPCUsernameOrPasswordAreInTheConfigFile)
{
    WriteConfigFile("a=b");
    teleport_local_server.LoadConfig(argc, argv);
    ASSERT_THROW(teleport_local_server.StartRPCServer(), std::runtime_error);
}

TEST_F(ATeleportLocalServer, ProvidesAMiningSeed)
{
    uint256 mining_seed = teleport_local_server.MiningSeed();
}

class ATeleportLocalServerWithAMiningRPCServer : public ATeleportLocalServer
{
public:
    HttpAuthServer *mining_http_server;
    MiningRPCServer *mining_rpc_server;
    HttpClient *mining_http_client;
    Client *mining_client;

    virtual void SetUp()
    {
        ATeleportLocalServer::SetUp();
        mining_http_server = new HttpAuthServer(8339, "username", "password");
        mining_rpc_server = new MiningRPCServer(*mining_http_server);
        mining_rpc_server->StartListening();
        mining_http_client = new HttpClient("http://localhost:8339");
        mining_http_client->AddHeader("Authorization", "Basic username:password");
        mining_client = new Client(*mining_http_client);
        WriteConfigFile("miningrpcport=8339\n"
                        "miningrpcuser=username\n"
                        "miningrpcpassword=password\n"
                        "rpcuser=teleportuser\n"
                        "rpcpassword=abcd123\n"
                        "rpcport=8383\n");
        teleport_local_server.LoadConfig(argc, argv);
    }

    virtual void TearDown()
    {
        ATeleportLocalServer::TearDown();
        mining_rpc_server->StopListening();
        delete mining_rpc_server;
        delete mining_http_server;
        delete mining_client;
        delete mining_http_client;
    }
};

TEST_F(ATeleportLocalServerWithAMiningRPCServer, SetsTheNetworkInfo)
{
    teleport_local_server.SetMiningNetworkInfo();
    auto seed_list = mining_client->CallMethod("list_seeds", Json::Value());
    ASSERT_THAT(seed_list.size(), Eq(1));
}

TEST_F(ATeleportLocalServerWithAMiningRPCServer, ReceivesTheNewProofNotificationFromTheMiner)
{
    teleport_local_server.SetMiningNetworkInfo();
    teleport_local_server.StartRPCServer();
    mining_client->CallMethod("start_mining", Json::Value());
    teleport_local_server.StopRPCServer();
    NetworkSpecificProofOfWork proof = teleport_local_server.GetLatestProofOfWork();
    ASSERT_TRUE(proof.IsValid());
}


class ATeleportLocalServerWithRPCSettings : public ATeleportLocalServer
{
public:
    HttpClient *http_client;
    Client *client;

    virtual void SetUp()
    {
        ATeleportLocalServer::SetUp();
        WriteConfigFile("rpcuser=teleportuser\n"
                        "rpcpassword=abcd123\n"
                        "rpcport=8383\n");
        teleport_local_server.LoadConfig(argc, argv);
        teleport_local_server.StartRPCServer();

        http_client = new HttpClient("http://localhost:8383");
        http_client->AddHeader("Authorization", "Basic teleportuser:abcd123");
        client = new Client(*http_client);
    }

    virtual void TearDown()
    {
        ATeleportLocalServer::TearDown();
        delete client;
        delete http_client;
        teleport_local_server.StopRPCServer();
    }
};

TEST_F(ATeleportLocalServerWithRPCSettings, StartsAnRPCServer)
{
    Json::Value params;

    auto result = client->CallMethod("help", params);
    ASSERT_THAT(result.size(), Ne(0));
}

TEST_F(ATeleportLocalServerWithRPCSettings, ProvidesABalance)
{
    Json::Value params;

    auto result = client->CallMethod("balance", params);
    ASSERT_THAT(result.asString(), Eq("0.00"));
}


class ATeleportLocalServerWithAMiningRPCServerAndATeleportNetworkNode : public ATeleportLocalServerWithAMiningRPCServer
{
public:
    HttpClient *http_client;
    Client *client;

    virtual void SetUp()
    {
        ATeleportLocalServerWithAMiningRPCServer::SetUp();
        teleport_local_server.SetMiningNetworkInfo();
        teleport_local_server.StartRPCServer();
        teleport_local_server.StartProofHandlerThread();

        http_client = new HttpClient("http://localhost:8383");
        http_client->AddHeader("Authorization", "Basic teleportuser:abcd123");
        client = new Client(*http_client);
    }

    virtual void TearDown()
    {
        teleport_local_server.StopProofHandlerThread();
        ATeleportLocalServerWithAMiningRPCServer::TearDown();
        delete client;
        delete http_client;
        teleport_local_server.StopRPCServer();
    }
};

TEST_F(ATeleportLocalServerWithAMiningRPCServerAndATeleportNetworkNode, StartsMining)
{
    client->CallMethod("start_mining", Json::Value());
    NetworkSpecificProofOfWork proof = teleport_local_server.GetLatestProofOfWork();
    ASSERT_TRUE(proof.IsValid());
}

TEST_F(ATeleportLocalServerWithAMiningRPCServerAndATeleportNetworkNode, AddsAMinedCreditMessageToTheTipWhenMined)
{
    client->CallMethod("start_mining", Json::Value());
    NetworkSpecificProofOfWork proof = teleport_local_server.GetLatestProofOfWork();
    ASSERT_TRUE(proof.IsValid());
    while (not teleport_local_server.latest_proof_was_handled)
        sleep(1);
    MinedCreditMessage tip = teleport_local_server.teleport_network_node->Tip();
    ASSERT_THAT(tip.proof_of_work, Eq(proof));
    ASSERT_THAT(proof.branch[0], Eq(tip.mined_credit.GetHash()));
}

