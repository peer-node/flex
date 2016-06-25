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

#include <src/base/util_hex.h>


using namespace ::testing;
using namespace jsonrpc;
using namespace std;

uint32_t port{8388};

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
        uint32_t this_port = port++;
        http_server = new HttpAuthServer(this_port, "username", "password");
        server = new FlexRPCServer(*http_server);
        server->StartListening();
        http_client = new HttpClient("http://localhost:" + PrintToString(this_port));
        http_client->AddHeader("Authorization", "Basic username:password");
        client = new Client(*http_client, JSONRPC_CLIENT_V2);
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
    ASSERT_THAT(result.asString(), Eq("0.00"));
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
        flex_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
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
    ASSERT_THAT(result.asString(), Eq("1.00"));
}

TEST_F(AFlexRPCServerWithAFlexNetworkNodeAndABalance, SendsCreditsToAPublicKey)
{
    Point public_key(SECP256K1, 2);
    string encoded_public_key = HexStr(public_key.getvch());
    parameters.append(encoded_public_key);
    parameters.append("1");
    auto result = client->CallMethod("sendtopublickey", parameters); // balance -1
    AddABatchToTheTip(); // balance +1
    result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asString(), Eq("1.00"));
}

TEST_F(AFlexRPCServerWithAFlexNetworkNodeAndABalance, ThrowsAnErrorIfABadPublicKeyIsGiven)
{
    parameters.append("00");
    parameters.append("1");
    EXPECT_ANY_THROW(client->CallMethod("sendtopublickey", parameters));
}

TEST_F(AFlexRPCServerWithAFlexNetworkNodeAndABalance, ThrowsAnErrorIfThereIsInsufficientBalance)
{
    parameters.append(HexStr(Point(SECP256K1, 2).getvch()));
    parameters.append("100");
    EXPECT_ANY_THROW(client->CallMethod("sendtopublickey", parameters));
}

TEST_F(AFlexRPCServerWithAFlexNetworkNodeAndABalance, GetsANewAddress)
{
    auto result = client->CallMethod("getnewaddress", parameters);
    uint64_t address_length = result.asString().size();
    bool length_ok = address_length == 34 or address_length == 33;
    ASSERT_TRUE(length_ok);
}

TEST_F(AFlexRPCServerWithAFlexNetworkNodeAndABalance, SendsCreditsToAnAddress)
{
    Point public_key(SECP256K1, 2);
    string address = GetAddressFromPublicKey(public_key);
    parameters.append(address);
    parameters.append("1");
    auto result = client->CallMethod("sendtoaddress", parameters); // balance -1
    AddABatchToTheTip(); // balance +1
    result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asString(), Eq("1.00"));
}

TEST_F(AFlexRPCServerWithAFlexNetworkNodeAndABalance, ThrowsAnErrorIfABadAddressIsGiven)
{
    string address = GetAddressFromPublicKey(Point(SECP256K1, 2));
    string bad_address = address + "1";
    parameters.append(bad_address);
    parameters.append("1");
    EXPECT_ANY_THROW(client->CallMethod("sendtopublickey", parameters));
}

class AFlexRPCServerWithCreditsSentToAnAddressWhosePrivateKeyIsKnown : public AFlexRPCServerWithAFlexNetworkNodeAndABalance
{
public:
    virtual void SetUp()
    {
        Json::Value parameters;
        AFlexRPCServerWithAFlexNetworkNodeAndABalance::SetUp();
        auto result = client->CallMethod("getnewaddress", parameters);
        auto address = result.asString();
        parameters.append(address);
        parameters.append("1");
        client->CallMethod("sendtoaddress", parameters); // balance -1 +1
        AddABatchToTheTip(); // balance +1
    }

    virtual void TearDown()
    {
        AFlexRPCServerWithAFlexNetworkNodeAndABalance::TearDown();
    }
};

TEST_F(AFlexRPCServerWithCreditsSentToAnAddressWhosePrivateKeyIsKnown, ReceivesTheCredits)
{
    auto result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asString(), Eq("2.00"));
}

TEST_F(AFlexRPCServerWithCreditsSentToAnAddressWhosePrivateKeyIsKnown, CanSpendTheCredits)
{
    Point public_key(SECP256K1, 2);
    parameters.append(HexStr(public_key.getvch()));
    parameters.append("2");
    auto result = client->CallMethod("sendtopublickey", parameters); // balance -2
    AddABatchToTheTip(); // balance +1
    result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asString(), Eq("1.00"));
}
