#include <test/teleport_tests/node/relays/messages/RelayJoinMessage.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/calendar/Calendar.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"
#include "test/teleport_tests/node/TestPeer.h"
#include "test/teleport_tests/node/TestPeerWithNetworkNode.h"
#include "test/teleport_tests/node/historical_data/handlers/TipHandler.h"
#include "test/teleport_tests/node/historical_data/handlers/CalendarHandler.h"
#include "test/teleport_tests/node/historical_data/handlers/InitialDataHandler.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"

class TestWithDataStreams : public Test
{
public:
    template <typename T>
    CDataStream GetDataStream(string channel, T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << channel << message.Type() << message;
        return ss;
    }
};

class ATeleportNetworkNode : public TestWithDataStreams
{
public:
    TeleportNetworkNode teleport_network_node;
    TestPeer peer;

    virtual void SetUp()
    {
        teleport_network_node.credit_message_handler->do_spot_checks = false;
        teleport_network_node.relay_message_handler->mode = BLOCK_VALIDATION;
    }

    void MarkProofAsValid(MinedCreditMessage msg)
    {
        teleport_network_node.credit_message_handler->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    }
};

TEST_F(ATeleportNetworkNode, StartsWithAnEmptyCalendar)
{
    Calendar calendar = teleport_network_node.GetCalendar();
    ASSERT_THAT(calendar.calends.size(), Eq(0));
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(0));
}

TEST_F(ATeleportNetworkNode, StartsWithAnEmptyWallet)
{
    ASSERT_THAT(teleport_network_node.wallet->credits.size(), Eq(0));
}

TEST_F(ATeleportNetworkNode, HandlesMinedCreditMessages)
{
    MinedCreditMessage msg = teleport_network_node.builder.GenerateMinedCreditMessageWithoutProofOfWork();
    MarkProofAsValid(msg);
    teleport_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), (CNode*)&peer);
    ASSERT_THAT(teleport_network_node.Tip(), Eq(msg));
}

TEST_F(ATeleportNetworkNode, IncreasesTheBalanceWhenItHandlesMinedCreditMessages)
{
    MinedCreditMessage msg = teleport_network_node.builder.GenerateMinedCreditMessageWithoutProofOfWork();
    MarkProofAsValid(msg);
    teleport_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), (CNode*)&peer);
    msg = teleport_network_node.builder.GenerateMinedCreditMessageWithoutProofOfWork();
    MarkProofAsValid(msg);
    teleport_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), (CNode*)&peer);
    ASSERT_THAT(teleport_network_node.Balance(), Eq(ONE_CREDIT));
}

TEST_F(ATeleportNetworkNode, RemembersMinedCreditMessagesWithoutProofsOfWorkThatItGenerates)
{
    MinedCreditMessage msg = teleport_network_node.GenerateMinedCreditMessageWithoutProofOfWork();
    uint256 credit_hash = msg.mined_credit.GetHash();
    MinedCreditMessage remembered_msg = teleport_network_node.RecallGeneratedMinedCreditMessage(credit_hash);
    ASSERT_THAT(remembered_msg, Eq(msg));
}


class ATeleportNetworkNodeWithABalance : public ATeleportNetworkNode
{
public:

    void AddABatchToTheTip()
    {
        auto msg = teleport_network_node.builder.GenerateMinedCreditMessageWithoutProofOfWork();
        MarkProofAsValid(msg);
        teleport_network_node.HandleMessage(string("credit"), GetDataStream("credit", msg), (CNode*)&peer);
    }

    virtual void SetUp()
    {
        ATeleportNetworkNode::SetUp();
        AddABatchToTheTip();
        AddABatchToTheTip();
    }

    virtual void TearDown()
    {
        ATeleportNetworkNode::TearDown();
    }
};

TEST_F(ATeleportNetworkNodeWithABalance, HasABalance)
{
    ASSERT_THAT(teleport_network_node.Balance(), Eq(ONE_CREDIT));
}

TEST_F(ATeleportNetworkNodeWithABalance, SendsCreditsToAPublicKey)
{
    Point pubkey(SECP256K1, 2);
    teleport_network_node.SendCreditsToPublicKey(pubkey, ONE_CREDIT); // balance -1
    AddABatchToTheTip();  // balance +1
    ASSERT_THAT(teleport_network_node.Balance(), Eq(ONE_CREDIT));
}

TEST_F(ATeleportNetworkNodeWithABalance, ReceivesCreditsSentToAPublicKeyWhosePrivateKeyIsKnown)
{
    Point pubkey(SECP256K1, 2);
    teleport_network_node.keydata[pubkey]["privkey"] = CBigNum(2);
    teleport_network_node.SendCreditsToPublicKey(pubkey, ONE_CREDIT); // balance -1 +1
    AddABatchToTheTip();  // balance +1
    ASSERT_THAT(teleport_network_node.Balance(), Eq(2 * ONE_CREDIT));
}

TEST_F(ATeleportNetworkNode, SendsDataMessagesToTheDataMessageHandler)
{
    TipRequestMessage dummy_request;
    TipMessage tip_message(dummy_request, &teleport_network_node.calendar); // unrequested tip
    teleport_network_node.HandleMessage(string("data"), GetDataStream("data", tip_message), (CNode*)&peer);
    bool rejected = teleport_network_node.msgdata[tip_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ATeleportNetworkNode, SendsRelayMessagesToTheRelayMessageHandler)
{
    RelayJoinMessage join;
    teleport_network_node.HandleMessage(string("relay"), GetDataStream("relay", join), (CNode*)&peer);
    bool rejected = teleport_network_node.msgdata[join.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}


class TwoTeleportNetworkNodesConnectedViaPeers : public TestWithDataStreams
{
public:
    TestPeerWithNetworkNode peer1, peer2;
    TeleportNetworkNode node1, node2;

    virtual void SetUp()
    {
        node1.credit_message_handler->do_spot_checks = false;
        node2.credit_message_handler->do_spot_checks = false;
        node1.credit_message_handler->send_join_messages = false;
        node2.credit_message_handler->send_join_messages = false;

        ConnectNodesViaPeers();
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
        MilliSleep(150);
    }

    virtual void AddABatchToTheTip(TeleportNetworkNode *teleport_network_node)
    {
        auto msg = teleport_network_node->builder.GenerateMinedCreditMessageWithoutProofOfWork();
        // only this node will accept the proof of work
        teleport_network_node->credit_message_handler->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
        teleport_network_node->HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }
};


class TwoTeleportNetworkNodesWithSomeBatchesConnectedViaPeers : public TwoTeleportNetworkNodesConnectedViaPeers
{
public:
    virtual void SetUp()
    {
        TwoTeleportNetworkNodesConnectedViaPeers::SetUp();
        AddABatchToTheTip(&node1);
        AddABatchToTheTip(&node1);

        AddABatchToTheTip(&node2);
    }

    virtual void TearDown()
    {
        TwoTeleportNetworkNodesConnectedViaPeers::TearDown();
    }
};


TEST_F(TwoTeleportNetworkNodesWithSomeBatchesConnectedViaPeers, PassMessagesToOneAnother)
{
    TipRequestMessage tip_request;
    node1.HandleMessage(string("data"), GetDataStream("data", tip_request), (CNode*)&peer2); // unrequested

    ASSERT_TRUE(peer2.HasReceived("data", "tip", TipMessage(tip_request, &node1.calendar)));
    bool rejected = node2.msgdata[TipMessage(tip_request, &node1.calendar).GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(TwoTeleportNetworkNodesConnectedViaPeers, SendTipRequests)
{
    uint160 request_hash = node2.data_message_handler->tip_handler->RequestTips();
    TipRequestMessage tip_request = node2.msgdata[request_hash]["tip_request"];
    ASSERT_TRUE(peer1.HasBeenInformedAbout("data", "tip_request", tip_request));
}

TEST_F(TwoTeleportNetworkNodesWithSomeBatchesConnectedViaPeers, SendTipsInResponseToRequests)
{
    uint160 request_hash = node2.data_message_handler->tip_handler->RequestTips();
    // give node2 a very high total work so that the tip message doesn't provoke a calendar request
    node2.calendar.current_diurn.credits_in_diurn.back().mined_credit.network_state.difficulty = 100000000;
    TipRequestMessage tip_request = node2.msgdata[request_hash]["tip_request"];
    MilliSleep(350);
    ASSERT_TRUE(peer2.HasReceived("data", "tip", TipMessage(tip_request, &node1.calendar)));
}


class TwoTeleportNetworkNodesWithValidProofsOfWorkConnectedViaPeers : public TwoTeleportNetworkNodesWithSomeBatchesConnectedViaPeers
{
public:

    void SetMiningPreferences(TeleportNetworkNode &node)
    {
        node.credit_system->initial_difficulty = 100;
        node.credit_system->initial_diurnal_difficulty = 500;
    }

    virtual void SetUp()
    {
        node1.credit_message_handler->send_join_messages = false;
        node2.credit_message_handler->send_join_messages = false;

        SetMiningPreferences(node1);
        SetMiningPreferences(node2);

        ConnectNodesViaPeers();
    }

    virtual void AddABatchToTheTip(TeleportNetworkNode *teleport_network_node)
    {
        auto msg = teleport_network_node->builder.GenerateMinedCreditMessageWithoutProofOfWork();
        CompleteProofOfWork(msg);
        teleport_network_node->HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }

    void CompleteProofOfWork(MinedCreditMessage& msg)
    {
        node1.credit_system->AddIncompleteProofOfWork(msg);
        uint8_t keep_working = 1;
        msg.proof_of_work.proof.DoWork(&keep_working);
    }
};

void WaitForCalendarSwitch(TeleportNetworkNode &node, uint160 last_tip=0, uint32_t millisecond_timeout=15000,
                           uint160 tip_to_wait_for=0)
{
    int64_t start_time = GetTimeMillis();
    log_ << "------------------- stating to wait for calendar to switch from " << last_tip << "\n";
    while (node.calendar.LastMinedCreditMessageHash() == last_tip
            or (tip_to_wait_for != 0 and node.calendar.LastMinedCreditMessageHash() != tip_to_wait_for))
    {
        MilliSleep(10);
        LOCK(node.credit_message_handler->calendar_mutex);
        if (GetTimeMillis() - start_time > millisecond_timeout)
        {
            log_ << "timed out waiting for calendar switch\n";
            return;
        }
    }
    log_ << "condition to stop waiting satisfied: " << (not (node.calendar.LastMinedCreditMessageHash() == last_tip
            or (tip_to_wait_for != 0 and node.calendar.LastMinedCreditMessageHash() != tip_to_wait_for))) << "\n";
    log_ << "tip to wait for: " << tip_to_wait_for << "\n";
    log_ << "tip: " << node.calendar.LastMinedCreditMessageHash() << "\n";
}

TEST_F(TwoTeleportNetworkNodesWithValidProofsOfWorkConnectedViaPeers, AreSynchronized)
{
    auto original_node2_tip = node2.calendar.LastMinedCreditMessageHash();


    AddABatchToTheTip(&node1);
    log_ << "going to wait for node 2 calendar to switch from " << original_node2_tip << "\n";
    WaitForCalendarSwitch(node2, original_node2_tip);

    auto intermediate_node2_tip = node2.calendar.LastMinedCreditMessageHash();
    AddABatchToTheTip(&node1);
    log_ << "going to wait for node 2 calendar to switch from " << intermediate_node2_tip << "\n";
    WaitForCalendarSwitch(node2, intermediate_node2_tip);


    auto intermediate_node1_tip = node1.calendar.LastMinedCreditMessageHash();
    AddABatchToTheTip(&node2);
    log_ << "going to wait for node 1 calendar to switch from " << intermediate_node1_tip << "\n";
    WaitForCalendarSwitch(node1, intermediate_node1_tip);

    ASSERT_THAT(node1.calendar.LastMinedCreditMessageHash(), Eq(node2.calendar.LastMinedCreditMessageHash()));

//    MinedCreditMessage msg = node1.calendar.LastMinedCreditMessage();
//    ASSERT_THAT(msg.mined_credit.network_state.batch_number, Eq(3));
}


class TwoTeleportNetworkNodesConnectedAfterBatchesAreAdded : public TwoTeleportNetworkNodesWithValidProofsOfWorkConnectedViaPeers
{
public:
    uint160 original_node2_tip;

    virtual void SetUp()
    {
        SetMiningPreferences(node1);
        SetMiningPreferences(node2);

        AddABatchToTheTip(&node1);
        AddABatchToTheTip(&node1);

        AddABatchToTheTip(&node2);

        original_node2_tip = node2.calendar.LastMinedCreditMessageHash();

        ConnectNodesViaPeers();
    }
};

TEST_F(TwoTeleportNetworkNodesConnectedAfterBatchesAreAdded, SynchronizeThroughSharingMinedCreditMessages)
{
    AddABatchToTheTip(&node1);
    WaitForCalendarSwitch(node2, original_node2_tip); // wait for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditMessageHash(), Eq(node2.calendar.LastMinedCreditMessageHash()));
}

TEST_F(TwoTeleportNetworkNodesConnectedAfterBatchesAreAdded, SynchronizeThroughAnInitialDataMessage)
{
    node2.data_message_handler->tip_handler->RequestTips();
    WaitForCalendarSwitch(node2, original_node2_tip); // wait for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditMessageHash(), Eq(node2.calendar.LastMinedCreditMessageHash()));
}


class TwoTeleportNetworkNodesConnectedAfterBatchesWithTransactionsAreAdded : public TwoTeleportNetworkNodesWithValidProofsOfWorkConnectedViaPeers
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


TEST_F(TwoTeleportNetworkNodesConnectedAfterBatchesWithTransactionsAreAdded, SynchronizeThroughSharingMinedCreditMessages)
{
    AddABatchToTheTip(&node1);
    uint160 tip_to_wait_for = node1.calendar.LastMinedCreditMessageHash();
    WaitForCalendarSwitch(node2, 0, 15000, tip_to_wait_for); // wait for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditMessageHash(), Eq(node2.calendar.LastMinedCreditMessageHash()));
    ASSERT_THAT(node1.Balance(), Eq(ONE_CREDIT)); // mined two credits, spent 1
    ASSERT_THAT(node2.Balance(), Eq(ONE_CREDIT)); // received from transaction
}

TEST_F(TwoTeleportNetworkNodesConnectedAfterBatchesWithTransactionsAreAdded, SynchronizeThroughAnInitialDataMessage)
{
    log_ << "######################### getting node2 to request tips\n";
    uint160 tip_to_wait_for = node1.calendar.LastMinedCreditMessageHash();
    node2.data_message_handler->tip_handler->RequestTips();
    WaitForCalendarSwitch(node2, 0, 15000, tip_to_wait_for); // wait for node2 to synchronize with node1

    ASSERT_THAT(node2.calendar.LastMinedCreditMessageHash(), Eq(node1.calendar.LastMinedCreditMessageHash()));
}

class TwoTeleportNetworkNodesConnectedAfterBatchesWithTransactionsAreAddedFollowedByCalends :
        public TwoTeleportNetworkNodesConnectedAfterBatchesWithTransactionsAreAdded
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

        AddABatchToTheTip(&node1);
        while (node1.calendar.current_diurn.Size() != 1)
        {
            AddABatchToTheTip(&node1);
        }

        ConnectNodesViaPeers();
    }
};


TEST_F(TwoTeleportNetworkNodesConnectedAfterBatchesWithTransactionsAreAddedFollowedByCalends, SynchronizeThroughSharingMinedCreditMessages)
{
    AddABatchToTheTip(&node1);
    uint160 tip_to_wait_for = node1.calendar.LastMinedCreditMessageHash();
    WaitForCalendarSwitch(node2, 0, 15000, tip_to_wait_for); // wait for node2 to synchronize with node1

    ASSERT_THAT(node2.calendar.LastMinedCreditMessageHash(), Eq(tip_to_wait_for));
}

TEST_F(TwoTeleportNetworkNodesConnectedAfterBatchesWithTransactionsAreAddedFollowedByCalends, SynchronizeThroughAnInitialDataMessage)
{
    log_ << "######################## getting node2 to request tips\n";
    uint160 tip_to_wait_for = node1.calendar.LastMinedCreditMessageHash();
    node2.data_message_handler->tip_handler->RequestTips();
    WaitForCalendarSwitch(node2, 0, 15000, tip_to_wait_for); // wait for node2 to synchronize with node1

    ASSERT_THAT(node2.calendar.LastMinedCreditMessageHash(), Eq(tip_to_wait_for));
}


class ATeleportNetworkNodeWithAConfig : public ATeleportNetworkNode
{
public:
    TeleportConfig config;

    virtual void SetUp()
    {
        config["port"] = PrintToString(10000 + GetRand(1000));
        teleport_network_node.config = &config;
    }
};

TEST_F(ATeleportNetworkNodeWithAConfig, StartsACommunicatorListening)
{
    ASSERT_TRUE(teleport_network_node.StartCommunicator());
    teleport_network_node.StopCommunicator();
}


class TwoTeleportNetworkNodesWithCommunicators : public TestWithDataStreams
{
public:
    TeleportNetworkNode node1, node2;
    TeleportConfig config1, config2;
    uint64_t port1, port2;

    virtual void SetUp()
    {
        node1.credit_message_handler->do_spot_checks = false;
        node2.credit_message_handler->do_spot_checks = false;

        port1 = 10000 + GetRand(1000);
        port2 = port1 + 1;
        config1["port"] = PrintToString(port1);
        config2["port"] = PrintToString(port2);

        node1.config = &config1;
        node2.config = &config2;

        node1.StartCommunicator();
        node2.StartCommunicator();
    }

    virtual void TearDown()
    {
        node1.StopCommunicator();
        node2.StopCommunicator();
    }

    virtual void AddABatchToTheTip(TeleportNetworkNode *teleport_network_node)
    {
        auto msg = teleport_network_node->builder.GenerateMinedCreditMessageWithoutProofOfWork();
        CompleteProofOfWork(msg);
        teleport_network_node->HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }

    void SetMiningPreferences(TeleportNetworkNode &node)
    {
        node.credit_system->initial_difficulty = 100;
        node.credit_system->initial_diurnal_difficulty = 500;
    }

    void CompleteProofOfWork(MinedCreditMessage& msg)
    {
        node1.credit_system->AddIncompleteProofOfWork(msg);
        uint8_t keep_working = 1;
        msg.proof_of_work.proof.DoWork(&keep_working);
    }
};

TEST_F(TwoTeleportNetworkNodesWithCommunicators, ConnectToEachOther)
{
    ASSERT_THAT(node1.communicator->network.vNodes.size(), Eq(0));
    ASSERT_THAT(node2.communicator->network.vNodes.size(), Eq(0));

    string node2_address = string("127.0.0.1:") + PrintToString(port2);
    node1.communicator->network.AddNode(node2_address);

    node1.communicator->network.vNodes[0]->WaitForVersion();
    node2.communicator->network.vNodes[0]->WaitForVersion();

    ASSERT_THAT(node1.communicator->network.vNodes.size(), Eq(1));
    ASSERT_THAT(node2.communicator->network.vNodes.size(), Eq(1));
}

class TwoTeleportNetworkNodesConnectedViaCommunicators : public TwoTeleportNetworkNodesWithCommunicators
{
public:

    virtual void SetUp()
    {
        TwoTeleportNetworkNodesWithCommunicators::SetUp();
        node1.communicator->network.AddNode("127.0.0.1:" + PrintToString(port2));
        SetMiningPreferences(node1);
        SetMiningPreferences(node2);
        MilliSleep(150);
    }
};

TEST_F(TwoTeleportNetworkNodesConnectedViaCommunicators, SendAndReceiveMessages)
{
    AddABatchToTheTip(&node1);
    WaitForCalendarSwitch(node2); // wait for node2 to synchronize with node1
    ASSERT_THAT(node1.calendar.LastMinedCreditMessageHash(), Eq(node2.calendar.LastMinedCreditMessageHash()));
}

