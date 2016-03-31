#include <jsonrpccpp/client.h>

#define private public // to allow access to HttpClient.curl
#include <jsonrpccpp/client/connectors/httpclient.h>
#undef private

#include <fstream>
#include <test/flex_tests/mining/MiningRPCServer.h>
#include "gmock/gmock.h"
#include "FlexNode.h"


using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class AFlexNode : public Test
{
public:
    FlexNode flexnode;
    string config_filename;
    int argc = 2;
    const char *argv[2] = {"", "-conf=flex_tmp_config_k5i4jn5u.conf"};

    virtual void SetUp()
    {
        config_filename = flexnode.config_parser.GetDataDir().string() + "/flex_tmp_config_k5i4jn5u.conf";
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

TEST_F(AFlexNode, ReadsCommandLineArgumentsAndConfig)
{
    WriteConfigFile("a=b");
    flexnode.LoadConfig(argc, argv);
    ASSERT_THAT(flexnode.config["a"], Eq("b"));
}

TEST_F(AFlexNode, ThrowsAnExceptionIfNoRPCUsernameOrPasswordAreInTheConfigFile)
{
    WriteConfigFile("a=b");
    flexnode.LoadConfig(argc, argv);
    ASSERT_THROW(flexnode.StartRPCServer(), std::runtime_error);
}

TEST_F(AFlexNode, ProvidesAMiningSeed)
{
    uint256 mining_seed = flexnode.MiningSeed();
}

class AFlexNodeWithAMiningRPCServer : public AFlexNode
{
public:
    HttpAuthServer *mining_http_server;
    MiningRPCServer *mining_rpc_server;
    HttpClient *mining_http_client;
    Client *mining_client;

    virtual void SetUp()
    {
        AFlexNode::SetUp();
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
        flexnode.LoadConfig(argc, argv);
    }

    virtual void TearDown()
    {
        AFlexNode::TearDown();
        mining_rpc_server->StopListening();
        delete mining_rpc_server;
        delete mining_http_server;
        delete mining_client;
        delete mining_http_client;
    }
};

TEST_F(AFlexNodeWithAMiningRPCServer, SetsTheNumberOfMegabytesUsed)
{
    flexnode.SetNumberOfMegabytesUsedForMining(1);
    auto result = mining_client->CallMethod("get_megabytes_used", Json::Value());
    ASSERT_THAT(result.asInt64(), Eq(1));
}

TEST_F(AFlexNodeWithAMiningRPCServer, SetsTheNetworkInfo)
{
    flexnode.SetMiningNetworkInfo();
    auto seed_list = mining_client->CallMethod("list_seeds", Json::Value());
    ASSERT_THAT(seed_list.size(), Eq(1));
}

TEST_F(AFlexNodeWithAMiningRPCServer, ReceivesTheNewProofNotificationFromTheMiner)
{
    flexnode.SetNumberOfMegabytesUsedForMining(1);
    flexnode.SetMiningNetworkInfo();
    flexnode.StartRPCServer();
    mining_client->CallMethod("start_mining", Json::Value());
    flexnode.StopRPCServer();
    NetworkSpecificProofOfWork proof = flexnode.GetLatestProofOfWork();
    ASSERT_THAT(proof.MegabytesUsed(), Eq(1));
    ASSERT_TRUE(proof.IsValid());
}


class AFlexNodeWithRPCSettings : public AFlexNode
{
public:
    HttpClient *http_client;
    Client *client;

    virtual void SetUp()
    {
        AFlexNode::SetUp();
        WriteConfigFile("rpcuser=flexuser\n"
                        "rpcpassword=abcd123\n"
                        "rpcport=8383\n");
        flexnode.LoadConfig(argc, argv);
        flexnode.StartRPCServer();

        http_client = new HttpClient("http://localhost:8383");
        http_client->AddHeader("x", "y");
        client = new Client(*http_client);
    }

    virtual void TearDown()
    {
        AFlexNode::TearDown();
        delete client;
        delete http_client;
        flexnode.StopRPCServer();
    }
};

TEST_F(AFlexNodeWithRPCSettings, StartsAnRPCServer)
{
    Json::Value params;

    auto result = client->CallMethod("help", params);
    ASSERT_THAT(result.asString(), Ne(""));
}