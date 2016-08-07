#include "gmock/gmock.h"
#include "Calendar.h"
#include "FlexNetworkNode.h"
#include "TestPeer.h"
#include "TestPeerWithNetworkNode.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"

class AFlexNetworkNode : public Test
{
public:
    FlexNetworkNode flex_network_node;
    TestPeer peer;

    template <typename T>
    CDataStream GetDataStream(string channel, T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << channel << message.Type() << message;
        return ss;
    }

    virtual void SetUp()
    {
        flex_network_node.credit_message_handler->do_spot_checks = false;
    }

    void MarkProofAsValid(MinedCreditMessage msg)
    {
        flex_network_node.credit_message_handler->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    }

};

TEST_F(AFlexNetworkNode, StartsWithAnEmptyCalendar)
{
    Calendar calendar = flex_network_node.GetCalendar();
    ASSERT_THAT(calendar.calends.size(), Eq(0));
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(0));
}

TEST_F(AFlexNetworkNode, StartsWithAnEmptyWallet)
{
    ASSERT_THAT(flex_network_node.wallet->credits.size(), Eq(0));
}

TEST_F(AFlexNetworkNode, HandlesMinedCreditMessages)
{
    MinedCreditMessage msg = flex_network_node.credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
    MarkProofAsValid(msg);
    flex_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), (CNode*)&peer);
    ASSERT_THAT(flex_network_node.Tip(), Eq(msg));
}

TEST_F(AFlexNetworkNode, IncreasesTheBalanceWhenItHandlesMinedCreditMessages)
{
    MinedCreditMessage msg = flex_network_node.credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
    MarkProofAsValid(msg);
    flex_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), (CNode*)&peer);
    msg = flex_network_node.credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
    MarkProofAsValid(msg);
    flex_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), (CNode*)&peer);
    ASSERT_THAT(flex_network_node.Balance(), Eq(ONE_CREDIT));
}

TEST_F(AFlexNetworkNode, RemembersMinedCreditMessagesWithoutProofsOfWorkThatItGenerates)
{
    MinedCreditMessage msg = flex_network_node.GenerateMinedCreditMessageWithoutProofOfWork();
    uint256 credit_hash = msg.mined_credit.GetHash();
    MinedCreditMessage remembered_msg = flex_network_node.RecallGeneratedMinedCreditMessage(credit_hash);
    ASSERT_THAT(remembered_msg, Eq(msg));
}

class AFlexNetworkNodeWithABalance : public AFlexNetworkNode
{
public:

    void AddABatchToTheTip()
    {
        MinedCreditMessage msg = flex_network_node.credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
        MarkProofAsValid(msg);
        flex_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), (CNode*)&peer);
    }

    virtual void SetUp()
    {
        AFlexNetworkNode::SetUp();
        AddABatchToTheTip();
        AddABatchToTheTip();
    }

    virtual void TearDown()
    {
        AFlexNetworkNode::TearDown();
    }
};

TEST_F(AFlexNetworkNodeWithABalance, HasABalance)
{
    ASSERT_THAT(flex_network_node.Balance(), Eq(ONE_CREDIT));
}

TEST_F(AFlexNetworkNodeWithABalance, SendsCreditsToAPublicKey)
{
    Point pubkey(SECP256K1, 2);
    flex_network_node.SendCreditsToPublicKey(pubkey, ONE_CREDIT); // balance -1
    AddABatchToTheTip();  // balance +1
    ASSERT_THAT(flex_network_node.Balance(), Eq(ONE_CREDIT));
}

TEST_F(AFlexNetworkNodeWithABalance, ReceivesCreditsSentToAPublicKeyWhosePrivateKeyIsKnown)
{
    Point pubkey(SECP256K1, 2);
    flex_network_node.keydata[pubkey]["privkey"] = CBigNum(2);
    flex_network_node.SendCreditsToPublicKey(pubkey, ONE_CREDIT); // balance -1 +1
    AddABatchToTheTip();  // balance +1
    ASSERT_THAT(flex_network_node.Balance(), Eq(2 * ONE_CREDIT));
}

TEST_F(AFlexNetworkNode, SendsDataMessagesToTheDataMessageHandler)
{
    TipRequestMessage dummy_request;
    TipMessage tip_message(dummy_request, &flex_network_node.calendar); // unrequested tip
    flex_network_node.HandleMessage(string("data"), GetDataStream("data", tip_message), (CNode*)&peer);
    bool rejected = flex_network_node.msgdata[tip_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

class TwoFlexNetworkNodesConnectedViaPeers : public Test
{
public:
    TestPeerWithNetworkNode peer1, peer2;
    FlexNetworkNode node1, node2;

    virtual void SetUp()
    {
        node1.credit_message_handler->do_spot_checks = false;
        node2.credit_message_handler->do_spot_checks = false;

        peer1.SetNetworkNode(&node1);
        peer2.SetNetworkNode(&node2);
        peer1.network.vNodes.push_back(&peer2);
        peer2.network.vNodes.push_back(&peer1);

        AddABatchToTheTip(&node1);
        AddABatchToTheTip(&node1);

        AddABatchToTheTip(&node2);
    }

    virtual void TearDown()
    { }

    template <typename T> CDataStream GetDataStream(string channel, T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << channel << message.Type() << message;
        return ss;
    }

    void AddABatchToTheTip(FlexNetworkNode *flex_network_node)
    {
        auto msg = flex_network_node->credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
        flex_network_node->credit_message_handler->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
        flex_network_node->HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }
};

TEST_F(TwoFlexNetworkNodesConnectedViaPeers, PassMessagesToOneAnother)
{
    TipRequestMessage tip_request;
    node1.HandleMessage(string("data"), GetDataStream("data", tip_request), (CNode*)&peer2);
    ASSERT_TRUE(peer2.HasReceived("data", "tip", TipMessage(tip_request, &node1.calendar)));
    ASSERT_FALSE(peer2.HasReceived("data", "tip", TipMessage(tip_request, &node2.calendar)));
    ASSERT_FALSE(peer1.HasReceived("data", "tip", TipMessage(tip_request, &node2.calendar)));
}