#include "gmock/gmock.h"
#include "FlexRPCServer.h"
#include "HttpAuthServer.h"
#include "FlexNetworkNode.h"
#include "FlexLocalServer.h"
#include "TestPeer.h"

#include <jsonrpccpp/client.h>

#define private public // to allow access to HttpClient.curl
#include <jsonrpccpp/client/connectors/httpclient.h>
#undef private


using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class AFlexRPCServer : public Test
{
public:
    HttpAuthServer *http_server;
    FlexRPCServer *server;
    HttpClient *http_client;
    Client *client;
    Json::Value parameters;

    virtual void SetUp()
    {
        http_server = new HttpAuthServer(8388, "username", "password");
        server = new FlexRPCServer(*http_server);
        server->StartListening();
        http_client = new HttpClient("http://localhost:8388");
        http_client->AddHeader("Authorization", "Basic username:password");
        client = new Client(*http_client);
    }

    virtual void TearDown()
    {
        delete client;
        delete http_client;
        server->StopListening();
        delete server;
        delete http_server;
    }
};

TEST_F(AFlexRPCServer, ProvidesHelp)
{
    auto result = client->CallMethod("help", parameters);
    ASSERT_THAT(result.asString(), Ne(""));
}

TEST_F(AFlexRPCServer, ProvidesANetworkIDWithInfo)
{
    auto result = client->CallMethod("getinfo", parameters);

    uint256 network_id(result["network_id"].asString());
    ASSERT_THAT(network_id, Eq(0));
}

TEST_F(AFlexRPCServer, SetsTheNetworkID)
{
    parameters.append(uint256(1).ToString());
    auto result = client->CallMethod("setnetworkid", parameters);
    ASSERT_THAT(result.asString(), Eq("ok"));

    result = client->CallMethod("getinfo", parameters);
    uint256 network_id(result["network_id"].asString());
    ASSERT_THAT(network_id, Eq(1));
}

class AFlexRPCServerWithAFlexNetworkNode : public AFlexRPCServer
{
public:
    FlexNetworkNode flex_network_node;
    FlexLocalServer flex_local_server;

    virtual void SetUp()
    {
        AFlexRPCServer::SetUp();
        flex_network_node.credit_message_handler->do_spot_checks = false;
        flex_local_server.SetNetworkNode(&flex_network_node);
        server->SetFlexLocalServer(&flex_local_server);
    }

    virtual void TearDown()
    {
        AFlexRPCServer::TearDown();
    }
};

TEST_F(AFlexRPCServerWithAFlexNetworkNode, ProvidesABalance)
{
    auto result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asDouble(), Eq(0));
}

class AFlexRPCServerWithAFlexNetworkNodeAndABalance : public AFlexRPCServerWithAFlexNetworkNode
{
public:
    TestPeer peer;

    template <typename T>
    CDataStream GetDataStream(string channel, T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << channel << message.Type() << message;
        return ss;
    }

    void AddABatchToTheTip()
    {
        auto msg = flex_network_node.credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
        MarkProofAsValid(msg);
        flex_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), (CNode*)&peer);
    }

    void MarkProofAsValid(MinedCreditMessage msg)
    {
        flex_network_node.credit_message_handler->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    }

    virtual void SetUp()
    {
        AFlexRPCServerWithAFlexNetworkNode::SetUp();
        AddABatchToTheTip();
        AddABatchToTheTip();
    }

    virtual void TearDown()
    {
        AFlexRPCServerWithAFlexNetworkNode::TearDown();
    }
};

TEST_F(AFlexRPCServerWithAFlexNetworkNodeAndABalance, HasABalance)
{
    auto result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asDouble(), Eq(1));
}
