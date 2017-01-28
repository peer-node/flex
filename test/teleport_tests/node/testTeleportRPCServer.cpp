#include "gmock/gmock.h"
#include "test/teleport_tests/node/server/TeleportRPCServer.h"
#include "test/teleport_tests/node/server/HttpAuthServer.h"
#include "TeleportNetworkNode.h"
#include "test/teleport_tests/node/server/TeleportLocalServer.h"
#include "TestPeer.h"
#include "test/teleport_tests/node/data_handler/CalendarHandler.h"
#include "test/teleport_tests/node/data_handler/InitialDataHandler.h"

#include <jsonrpccpp/client.h>

#define private public // to allow access to HttpClient.curl
#include <jsonrpccpp/client/connectors/httpclient.h>
#undef private

#include <src/base/util_hex.h>


#include "log.h"
#define LOG_CATEGORY "test"

using namespace ::testing;
using namespace jsonrpc;
using namespace std;

uint32_t port{8388};

class ATeleportRPCServer : public Test
{
public:
    HttpAuthServer *http_server;
    TeleportRPCServer *server;
    HttpClient *http_client;
    Client *client;
    Json::Value parameters;
    uint32_t this_port;

    virtual void SetUp()
    {
        this_port = port++;
        http_server = new HttpAuthServer(this_port, "username", "password");
        server = new TeleportRPCServer(*http_server);
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


    template <typename T>
    CDataStream GetDataStream(string channel, T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << channel << message.Type() << message;
        return ss;
    }
};

TEST_F(ATeleportRPCServer, ProvidesHelp)
{
    auto result = client->CallMethod("help", parameters);
    ASSERT_THAT(result.size(), Eq(server->methods.size()));
}

TEST_F(ATeleportRPCServer, ProvidesANetworkIDWithInfo)
{
    auto result = client->CallMethod("getinfo", parameters);

    uint256 network_id(result["network_id"].asString());
    ASSERT_THAT(network_id, Eq(0));
}

TEST_F(ATeleportRPCServer, SetsTheNetworkID)
{
    parameters.append(uint256(1).ToString());
    auto result = client->CallMethod("setnetworkid", parameters);
    ASSERT_THAT(result.asString(), Eq("ok"));

    result = client->CallMethod("getinfo", parameters);
    uint256 network_id(result["network_id"].asString());
    ASSERT_THAT(network_id, Eq(1));
}

class ATeleportRPCServerWithATeleportNetworkNode : public ATeleportRPCServer
{
public:
    TeleportNetworkNode teleport_network_node;
    TeleportLocalServer teleport_local_server;

    virtual void SetUp()
    {
        ATeleportRPCServer::SetUp();
        teleport_network_node.credit_message_handler->do_spot_checks = false;
        teleport_local_server.SetNetworkNode(&teleport_network_node);
        TeleportConfig &config = *teleport_network_node.config;
        config["port"] = PrintToString(port++);
        server->SetTeleportLocalServer(&teleport_local_server);
    }

    virtual void TearDown()
    {
        ATeleportRPCServer::TearDown();
    }
};

TEST_F(ATeleportRPCServerWithATeleportNetworkNode, ProvidesABalance)
{
    auto result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asString(), Eq("0.00"));
}

class ATeleportRPCServerWithATeleportNetworkNodeAndABalance : public ATeleportRPCServerWithATeleportNetworkNode
{
public:
    TestPeer peer;


    void AddABatchToTheTip()
    {
        auto msg = teleport_network_node.builder.GenerateMinedCreditMessageWithoutProofOfWork();
        MarkProofAsValid(msg);
        teleport_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }

    void MarkProofAsValid(MinedCreditMessage msg)
    {
        teleport_network_node.credit_message_handler->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    }

    virtual void SetUp()
    {
        ATeleportRPCServerWithATeleportNetworkNode::SetUp();
        AddABatchToTheTip();
        AddABatchToTheTip();
    }

    virtual void TearDown()
    {
       ATeleportRPCServerWithATeleportNetworkNode::TearDown();
    }
};

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, HasABalance)
{
    auto result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asString(), Eq("1.00"));
}

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, ProvidesACalendar)
{
    auto result = client->CallMethod("getcalendar", parameters);
    uint64_t diurn_size = result["current_diurn"].size();
    ASSERT_THAT(diurn_size, Gt(0));
}

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, ProvidesAMinedCreditByHash)
{
    uint160 last_credit_hash = teleport_network_node.calendar.LastMinedCreditMessageHash();
    parameters.append(last_credit_hash.ToString());
    auto result = client->CallMethod("getminedcredit", parameters);

    ASSERT_TRUE(result["network_state"].isObject());
}

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, ListsUnspentCredits)
{
    auto result = client->CallMethod("listunspent", parameters);
    ASSERT_THAT(result.size(), Eq(1));
}

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, SendsCreditsToAPublicKey)
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

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, SendsANonIntegerNumberOfCreditsToAPublicKey)
{
    Point public_key(SECP256K1, 2);
    string encoded_public_key = HexStr(public_key.getvch());
    parameters.append(encoded_public_key);
    parameters.append("0.5");
    auto result = client->CallMethod("sendtopublickey", parameters); // balance -0.5
    AddABatchToTheTip(); // balance +1
    result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asString(), Eq("1.50"));
}

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, ThrowsAnErrorIfABadPublicKeyIsGiven)
{
    parameters.append("00");
    parameters.append("1");
    EXPECT_ANY_THROW(client->CallMethod("sendtopublickey", parameters));
}

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, ThrowsAnErrorIfThereIsInsufficientBalance)
{
    parameters.append(HexStr(Point(SECP256K1, 2).getvch()));
    parameters.append("100");
    EXPECT_ANY_THROW(client->CallMethod("sendtopublickey", parameters));
}

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, GetsANewAddress)
{
    auto result = client->CallMethod("getnewaddress", parameters);
    uint64_t address_length = result.asString().size();
    bool length_ok = address_length == 34 or address_length == 33;
    ASSERT_TRUE(length_ok);
}

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, SendsCreditsToAnAddress)
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


TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, GetsTransactions)
{
    Point public_key(SECP256K1, 2);
    string address = GetAddressFromPublicKey(public_key);
    parameters.append(address);
    parameters.append("1");
    auto result = client->CallMethod("sendtoaddress", parameters);
    std::string tx_hash = result.asString();
    AddABatchToTheTip();

    parameters.resize(0);
    parameters.append(tx_hash);
    result = client->CallMethod("gettransaction", parameters);
    ASSERT_THAT(result["rawtx"]["inputs"].size(), Gt(0));
}

TEST_F(ATeleportRPCServerWithATeleportNetworkNodeAndABalance, ThrowsAnErrorIfABadAddressIsGiven)
{
    string address = GetAddressFromPublicKey(Point(SECP256K1, 2));
    string bad_address = address + "1";
    parameters.append(bad_address);
    parameters.append("1");
    EXPECT_ANY_THROW(client->CallMethod("sendtopublickey", parameters));
}

class ATeleportRPCServerWithCreditsSentToAnAddressWhosePrivateKeyIsKnown : public ATeleportRPCServerWithATeleportNetworkNodeAndABalance
{
public:
    virtual void SetUp()
    {
        Json::Value parameters;
        ATeleportRPCServerWithATeleportNetworkNodeAndABalance::SetUp();
        auto result = client->CallMethod("getnewaddress", parameters);
        auto address = result.asString();
        parameters.append(address);
        parameters.append("1");
        client->CallMethod("sendtoaddress", parameters); // balance -1 +1
        AddABatchToTheTip(); // balance +1
    }

    virtual void TearDown()
    {
        ATeleportRPCServerWithATeleportNetworkNodeAndABalance::TearDown();
    }
};

TEST_F(ATeleportRPCServerWithCreditsSentToAnAddressWhosePrivateKeyIsKnown, ReceivesTheCredits)
{
    auto result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asString(), Eq("2.00"));
}

TEST_F(ATeleportRPCServerWithCreditsSentToAnAddressWhosePrivateKeyIsKnown, CanSpendTheCredits)
{
    Point public_key(SECP256K1, 2);
    parameters.append(HexStr(public_key.getvch()));
    parameters.append("2");
    auto result = client->CallMethod("sendtopublickey", parameters); // balance -2
    AddABatchToTheTip(); // balance +1
    result = client->CallMethod("balance", parameters);
    ASSERT_THAT(result.asString(), Eq("1.00"));
}


class ATeleportRPCServerWithARemoteTeleportNetworkNode : public ATeleportRPCServerWithATeleportNetworkNode
{
public:
    TeleportNetworkNode remote_network_node;
    TeleportConfig remote_config;
    uint64_t remote_node_port;

    virtual void SetUp()
    {
        ATeleportRPCServerWithATeleportNetworkNode::SetUp();
        remote_node_port = port++;
        remote_config["port"] = PrintToString(remote_node_port);
        remote_network_node.config = &remote_config;

        teleport_network_node.credit_message_handler->do_spot_checks = false;
        remote_network_node.credit_message_handler->do_spot_checks = false;

        teleport_network_node.StartCommunicator();
        remote_network_node.StartCommunicator();
    }

    virtual void TearDown()
    {
        teleport_network_node.StopCommunicator();
        remote_network_node.StopCommunicator();
        ATeleportRPCServerWithATeleportNetworkNode::TearDown();
    }
};


TEST_F(ATeleportRPCServerWithARemoteTeleportNetworkNode, ConnectsToTheRemoteNode)
{
    parameters.append(std::string("127.0.0.1:" + PrintToString(remote_node_port)));

    auto result = client->CallMethod("addnode", parameters);
    MilliSleep(200);
    teleport_network_node.communicator->Node(0)->WaitForVersion();

    ASSERT_THAT(remote_network_node.communicator->network.vNodes.size(), Eq(1));
}


class ATeleportRPCServerConnectedToARemoteTeleportNetworkNode : public ATeleportRPCServerWithARemoteTeleportNetworkNode
{
public:
    virtual void SetUp()
    {
        ATeleportRPCServerWithARemoteTeleportNetworkNode::SetUp();
        parameters.append(std::string("127.0.0.1:" + PrintToString(remote_node_port)));

        SetMiningPreferences(teleport_network_node);
        SetMiningPreferences(remote_network_node);

        auto result = client->CallMethod("addnode", parameters);
        MilliSleep(200);
        teleport_network_node.communicator->Node(0)->WaitForVersion();
        parameters = Json::Value();
    }

    virtual void AddABatchToTheTip(TeleportNetworkNode *teleport_network_node)
    {
        auto msg = teleport_network_node->builder.GenerateMinedCreditMessageWithoutProofOfWork();
        CompleteProofOfWork(msg);
        teleport_network_node->HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }

    void SetMiningPreferences(TeleportNetworkNode &node)
    {
        node.credit_system->SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(1);
        node.credit_system->initial_difficulty = 100;
        node.credit_system->initial_diurnal_difficulty = 500;
        node.data_message_handler->initial_data_handler->SetMiningParametersForInitialDataMessageValidation(1, 100, 500);
        node.data_message_handler->calendar_handler->calendar_scrutiny_time = 1 * 10000;
    }

    void CompleteProofOfWork(MinedCreditMessage& msg)
    {
        teleport_network_node.credit_system->AddIncompleteProofOfWork(msg);
        uint8_t keep_working = 1;
        msg.proof_of_work.proof.DoWork(&keep_working);
    }
};

TEST_F(ATeleportRPCServerConnectedToARemoteTeleportNetworkNode, SynchronizesWithTheRemoteNetworkNode)
{
    ASSERT_THAT(teleport_network_node.calendar.LastMinedCreditMessageHash(), Eq(0));

    AddABatchToTheTip(&teleport_network_node);
    MilliSleep(300);

    ASSERT_THAT(teleport_network_node.calendar.LastMinedCreditMessageHash(), Ne(0));
    ASSERT_THAT(teleport_network_node.calendar.LastMinedCreditMessageHash(), Eq(
            remote_network_node.calendar.LastMinedCreditMessageHash()));
}

TEST_F(ATeleportRPCServerConnectedToARemoteTeleportNetworkNode, SynchronizesWithTheRemoteNetworkNodeViaATipRequest)
{
    teleport_network_node.StopCommunicator();

    for (int i = 0; i < 4; i++)
        AddABatchToTheTip(&remote_network_node); // remote node builds a blockchain while disconnected from local node

    teleport_network_node.StartCommunicator();
    parameters.append(std::string("127.0.0.1:" + PrintToString(remote_node_port)));
    auto result = client->CallMethod("addnode", parameters); // reconnect

    MilliSleep(150);
    client->CallMethod("requesttips", parameters);

    MilliSleep(800);

    ASSERT_THAT(teleport_network_node.calendar.LastMinedCreditMessageHash(), Ne(0));
    ASSERT_THAT(teleport_network_node.calendar.LastMinedCreditMessageHash(), Eq(
            remote_network_node.calendar.LastMinedCreditMessageHash()));
}