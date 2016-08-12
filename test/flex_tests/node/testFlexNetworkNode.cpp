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
        auto msg = flex_network_node.credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
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

        ConnectNodesViaPeers();

        AddABatchToTheTip(&node1);
        AddABatchToTheTip(&node1);

        AddABatchToTheTip(&node2);
    }

    void ConnectNodesViaPeers()
    {
        peer1.id = 1;
        peer2.id = 2;

        peer1.SetNetworkNode(&node1);
        peer2.SetNetworkNode(&node2);
        peer1.network.vNodes.push_back(&peer2);
        peer2.network.vNodes.push_back(&peer1);

        node1.credit_message_handler->SetNetwork(peer1.network);
        node2.credit_message_handler->SetNetwork(peer2.network);
        node1.data_message_handler->SetNetwork(peer1.network);
        node2.data_message_handler->SetNetwork(peer2.network);
    }

    virtual void TearDown()
    {
        peer1.StopGettingMessages();
        peer2.StopGettingMessages();
        MilliSleep(100);
    }

    template <typename T> CDataStream GetDataStream(string channel, T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << channel << message.Type() << message;
        return ss;
    }

    virtual void AddABatchToTheTip(FlexNetworkNode *flex_network_node)
    {
        auto msg = flex_network_node->credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
        // only this node will accept the proof of work
        flex_network_node->credit_message_handler->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
        flex_network_node->HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }
};

TEST_F(TwoFlexNetworkNodesConnectedViaPeers, PassMessagesToOneAnother)
{
    TipRequestMessage tip_request;
    node1.HandleMessage(string("data"), GetDataStream("data", tip_request), (CNode*)&peer2); // unrequested

    ASSERT_TRUE(peer2.HasReceived("data", "tip", TipMessage(tip_request, &node1.calendar)));
    bool rejected = node2.msgdata[TipMessage(tip_request, &node1.calendar).GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(TwoFlexNetworkNodesConnectedViaPeers, SendTipRequests)
{
    uint160 request_hash = node2.data_message_handler->RequestTips();
    TipRequestMessage tip_request = node2.msgdata[request_hash]["tip_request"];
    ASSERT_TRUE(peer1.HasBeenInformedAbout("data", "tip_request", tip_request));
}

TEST_F(TwoFlexNetworkNodesConnectedViaPeers, SendTipsInResponseToRequests)
{
    uint160 request_hash = node2.data_message_handler->RequestTips();
    // give node2 a very high total work so that the tip message doesn't provoke a calendar request
    node2.calendar.current_diurn.credits_in_diurn.back().mined_credit.network_state.difficulty = 100000000;
    TipRequestMessage tip_request = node2.msgdata[request_hash]["tip_request"];
    MilliSleep(50);
    ASSERT_TRUE(peer2.HasReceived("data", "tip", TipMessage(tip_request, &node1.calendar)));
}

class TwoFlexNetworkNodesWithValidProofsOfWorkConnectedViaPeers : public TwoFlexNetworkNodesConnectedViaPeers
{
public:

    void SetMiningPreferences(FlexNetworkNode &node)
    {
        node.credit_system->SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(1);
        node.credit_system->initial_difficulty = 100;
        node.credit_system->initial_diurnal_difficulty = 500;
        node.data_message_handler->SetMiningParametersForInitialDataMessageValidation(1, 100, 500);
        node.data_message_handler->calendar_scrutiny_time = 1 * 10000;
    }

    virtual void SetUp()
    {
        SetMiningPreferences(node1);
        SetMiningPreferences(node2);

        ConnectNodesViaPeers();
    }

    virtual void AddABatchToTheTip(FlexNetworkNode *flex_network_node)
    {
        auto msg = flex_network_node->credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
        CompleteProofOfWork(msg);
        flex_network_node->HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }

    void CompleteProofOfWork(MinedCreditMessage& msg)
    {
        node1.credit_system->AddIncompleteProofOfWork(msg);
        uint8_t keep_working = 1;
        msg.proof_of_work.proof.DoWork(&keep_working);
    }
};

TEST_F(TwoFlexNetworkNodesWithValidProofsOfWorkConnectedViaPeers, AreSynchronized)
{
    AddABatchToTheTip(&node1);
    AddABatchToTheTip(&node1);

    MilliSleep(200); // time for node2 to synchronize with node1
    AddABatchToTheTip(&node2);

    MilliSleep(200); // time for node1 to synchronize with node2

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));

    MinedCredit mined_credit = node1.calendar.LastMinedCredit();
    ASSERT_THAT(mined_credit.network_state.batch_number, Eq(3));
}


class TwoFlexNetworkNodesConnectedAfterBatchesAreAdded : public TwoFlexNetworkNodesWithValidProofsOfWorkConnectedViaPeers
{
public:
    virtual void SetUp()
    {
        SetMiningPreferences(node1);
        SetMiningPreferences(node2);

        AddABatchToTheTip(&node1);
        AddABatchToTheTip(&node1);

        AddABatchToTheTip(&node2);

        ConnectNodesViaPeers();
    }
};

TEST_F(TwoFlexNetworkNodesConnectedAfterBatchesAreAdded, SynchronizeThroughSharingMinedCreditMessages)
{
    AddABatchToTheTip(&node1);
    MilliSleep(100); // time for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
}

TEST_F(TwoFlexNetworkNodesConnectedAfterBatchesAreAdded, SynchronizeThroughAnInitialDataMessage)
{
    node2.data_message_handler->RequestTips();
    MilliSleep(100); // time for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
}

class TwoFlexNetworkNodesConnectedAfterBatchesWithTransactionsAreAdded : public TwoFlexNetworkNodesWithValidProofsOfWorkConnectedViaPeers
{
public:
    virtual void SetUp()
    {
        SetMiningPreferences(node1);
        SetMiningPreferences(node2);

        AddABatchToTheTip(&node1);

        AddABatchToTheTip(&node1);
        Point pubkey = node2.GetNewPublicKey();
        node1.SendCreditsToPublicKey(pubkey, ONE_CREDIT);

        AddABatchToTheTip(&node2);

        ConnectNodesViaPeers();
    }
};


TEST_F(TwoFlexNetworkNodesConnectedAfterBatchesWithTransactionsAreAdded, SynchronizeThroughSharingMinedCreditMessages)
{
    AddABatchToTheTip(&node1);
    MilliSleep(100); // time for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
    ASSERT_THAT(node1.Balance(), Eq(ONE_CREDIT)); // mined two credits, spent 1
    ASSERT_THAT(node2.Balance(), Eq(ONE_CREDIT)); // received from transaction
}

TEST_F(TwoFlexNetworkNodesConnectedAfterBatchesWithTransactionsAreAdded, SynchronizeThroughAnInitialDataMessage)
{
    node2.data_message_handler->RequestTips();
    MilliSleep(100); // time for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
}