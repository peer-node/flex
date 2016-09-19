#include "gmock/gmock.h"
#include "Calendar.h"
#include "FlexNetworkNode.h"
#include "TestPeer.h"
#include "TestPeerWithNetworkNode.h"

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

class AFlexNetworkNode : public TestWithDataStreams
{
public:
    FlexNetworkNode flex_network_node;
    TestPeer peer;

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

class TwoFlexNetworkNodesConnectedViaPeers : public TestWithDataStreams
{
public:
    TestPeerWithNetworkNode peer1, peer2;
    FlexNetworkNode node1, node2;

    virtual void SetUp()
    {
        node1.credit_message_handler->do_spot_checks = false;
        node2.credit_message_handler->do_spot_checks = false;

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

    virtual void AddABatchToTheTip(FlexNetworkNode *flex_network_node)
    {
        auto msg = flex_network_node->credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
        // only this node will accept the proof of work
        flex_network_node->credit_message_handler->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
        flex_network_node->HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }
};


class TwoFlexNetworkNodesWithSomeBatchesConnectedViaPeers : public TwoFlexNetworkNodesConnectedViaPeers
{
public:
    virtual void SetUp()
    {
        TwoFlexNetworkNodesConnectedViaPeers::SetUp();
        AddABatchToTheTip(&node1);
        AddABatchToTheTip(&node1);

        AddABatchToTheTip(&node2);
    }

    virtual void TearDown()
    {
        TwoFlexNetworkNodesConnectedViaPeers::TearDown();
    }
};


TEST_F(TwoFlexNetworkNodesWithSomeBatchesConnectedViaPeers, PassMessagesToOneAnother)
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

TEST_F(TwoFlexNetworkNodesWithSomeBatchesConnectedViaPeers, SendTipsInResponseToRequests)
{
    uint160 request_hash = node2.data_message_handler->RequestTips();
    // give node2 a very high total work so that the tip message doesn't provoke a calendar request
    node2.calendar.current_diurn.credits_in_diurn.back().mined_credit.network_state.difficulty = 100000000;
    TipRequestMessage tip_request = node2.msgdata[request_hash]["tip_request"];
    MilliSleep(150);
    ASSERT_TRUE(peer2.HasReceived("data", "tip", TipMessage(tip_request, &node1.calendar)));
}

class TwoFlexNetworkNodesWithValidProofsOfWorkConnectedViaPeers : public TwoFlexNetworkNodesWithSomeBatchesConnectedViaPeers
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

void WaitForCalendarSwitch(FlexNetworkNode &node, uint160 last_tip = 0)
{
    if (last_tip == 0)
        last_tip = node.calendar.LastMinedCreditHash();
    while (node.calendar.LastMinedCreditHash() == last_tip)
    {
        MilliSleep(10);
        LOCK(node.credit_message_handler->calendar_mutex);
    }
}

TEST_F(TwoFlexNetworkNodesWithValidProofsOfWorkConnectedViaPeers, AreSynchronized)
{
    auto original_node2_tip = node2.calendar.LastMinedCreditHash();

    AddABatchToTheTip(&node1);
    WaitForCalendarSwitch(node2, original_node2_tip);

    auto intermediate_node2_tip = node2.calendar.LastMinedCreditHash();
    AddABatchToTheTip(&node1);
    WaitForCalendarSwitch(node2, intermediate_node2_tip);


    auto intermediate_node1_tip = node1.calendar.LastMinedCreditHash();
    AddABatchToTheTip(&node2);
    WaitForCalendarSwitch(node1, intermediate_node1_tip);

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
    WaitForCalendarSwitch(node2); // wait for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
}

TEST_F(TwoFlexNetworkNodesConnectedAfterBatchesAreAdded, SynchronizeThroughAnInitialDataMessage)
{
    node2.data_message_handler->RequestTips();
    WaitForCalendarSwitch(node2); // wait for node2 to synchronize with node1

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
    WaitForCalendarSwitch(node2); // wait for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
    ASSERT_THAT(node1.Balance(), Eq(ONE_CREDIT)); // mined two credits, spent 1
    ASSERT_THAT(node2.Balance(), Eq(ONE_CREDIT)); // received from transaction
}

TEST_F(TwoFlexNetworkNodesConnectedAfterBatchesWithTransactionsAreAdded, SynchronizeThroughAnInitialDataMessage)
{
    node2.data_message_handler->RequestTips();
    WaitForCalendarSwitch(node2); // wait for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
}

class TwoFlexNetworkNodesConnectedAfterBatchesWithTransactionsAreAddedFollowedByCalends :
        public TwoFlexNetworkNodesConnectedAfterBatchesWithTransactionsAreAdded
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


TEST_F(TwoFlexNetworkNodesConnectedAfterBatchesWithTransactionsAreAddedFollowedByCalends, SynchronizeThroughSharingMinedCreditMessages)
{
    AddABatchToTheTip(&node1);
    WaitForCalendarSwitch(node2); // wait for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
}

TEST_F(TwoFlexNetworkNodesConnectedAfterBatchesWithTransactionsAreAddedFollowedByCalends, SynchronizeThroughAnInitialDataMessage)
{
    node2.data_message_handler->RequestTips();
    WaitForCalendarSwitch(node2); // wait for node2 to synchronize with node1

    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
}


class AFlexNetworkNodeWithAConfig : public AFlexNetworkNode
{
public:
    FlexConfig config;

    virtual void SetUp()
    {
        config["port"] = PrintToString(10000 + GetRand(1000));
        flex_network_node.config = &config;
    }
};

TEST_F(AFlexNetworkNodeWithAConfig, StartsACommunicatorListening)
{
    ASSERT_TRUE(flex_network_node.StartCommunicator());
    flex_network_node.StopCommunicator();
}


class TwoFlexNetworkNodesWithCommunicators : public TestWithDataStreams
{
public:
    FlexNetworkNode node1, node2;
    FlexConfig config1, config2;
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

    virtual void AddABatchToTheTip(FlexNetworkNode *flex_network_node)
    {
        auto msg = flex_network_node->credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
        CompleteProofOfWork(msg);
        flex_network_node->HandleMessage(string("credit"), GetDataStream("credit", msg), NULL);
    }

    void SetMiningPreferences(FlexNetworkNode &node)
    {
        node.credit_system->SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(1);
        node.credit_system->initial_difficulty = 100;
        node.credit_system->initial_diurnal_difficulty = 500;
        node.data_message_handler->SetMiningParametersForInitialDataMessageValidation(1, 100, 500);
        node.data_message_handler->calendar_scrutiny_time = 1 * 10000;
    }

    void CompleteProofOfWork(MinedCreditMessage& msg)
    {
        node1.credit_system->AddIncompleteProofOfWork(msg);
        uint8_t keep_working = 1;
        msg.proof_of_work.proof.DoWork(&keep_working);
    }
};

TEST_F(TwoFlexNetworkNodesWithCommunicators, ConnectToEachOther)
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

class TwoFlexNetworkNodesConnectedViaCommunicators : public TwoFlexNetworkNodesWithCommunicators
{
public:

    virtual void SetUp()
    {
        TwoFlexNetworkNodesWithCommunicators::SetUp();
        node1.communicator->network.AddNode("127.0.0.1:" + PrintToString(port2));
        SetMiningPreferences(node1);
        SetMiningPreferences(node2);
        MilliSleep(150);
    }
};

TEST_F(TwoFlexNetworkNodesConnectedViaCommunicators, SendAndReceiveMessages)
{
    AddABatchToTheTip(&node1);
    WaitForCalendarSwitch(node2); // wait for node2 to synchronize with node1
    ASSERT_THAT(node1.calendar.LastMinedCreditHash(), Eq(node2.calendar.LastMinedCreditHash()));
}

class TwoFlexNetworkNodesConnectedViaCommunicatorsAfterOneHasBuiltACalendarWithAFailure :
        public TwoFlexNetworkNodesWithCommunicators
{
public:
    uint64_t bad_batch_number;

    virtual void SetUp()
    {
        TwoFlexNetworkNodesWithCommunicators::SetUp();
        SetMiningPreferences(node1);
        SetMiningPreferences(node2);

        BuildACalendarWithAFailure(node1);
        node1.communicator->network.AddNode("127.0.0.1:" + PrintToString(port2));

        node1.communicator->network.vNodes[0]->WaitForVersion();
        node2.communicator->network.vNodes[0]->WaitForVersion();
    }

    void BuildACalendarWithAFailure(FlexNetworkNode &node)
    {
        while (node.calendar.calends.size() < 2)
            AddABatchToTheTip(&node);

        MinedCreditMessage calend = node.calendar.calends.back();
        node.credit_message_handler->calendar->RemoveLast(node.credit_system);

        calend.proof_of_work.proof.link_lengths[2] += 1;
        node.credit_message_handler->AddToTip(calend);
        bad_batch_number = calend.mined_credit.network_state.batch_number;

        while (node.calendar.calends.size() < 5)
            AddABatchToTheTip(&node);
    }

    virtual void TearDown()
    {
        TwoFlexNetworkNodesWithCommunicators::TearDown();
    }
};

TEST_F(TwoFlexNetworkNodesConnectedViaCommunicatorsAfterOneHasBuiltACalendarWithAFailure, HaveDifferentCalendars)
{
    ASSERT_THAT(node1.calendar, Ne(node2.calendar));
}

TEST_F(TwoFlexNetworkNodesConnectedViaCommunicatorsAfterOneHasBuiltACalendarWithAFailure, HaveACalendarWithAFailure)
{
    bool ok = true;
    CalendarFailureDetails details;
    while (ok)
        ok = node1.calendar.SpotCheckWork(details);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(TwoFlexNetworkNodesConnectedViaCommunicatorsAfterOneHasBuiltACalendarWithAFailure,
       SynchronizeToTheLastGoodMinedCreditIfTheFailureIsDetected)
{
    Calendar original_calendar = node1.calendar;
    node2.data_message_handler->calendar_scrutiny_time = 100 * CALENDAR_SCRUTINY_TIME; // error will be found
    node2.data_message_handler->RequestTips(); // node2 should inform node1 of the error

    WaitForCalendarSwitch(node1, original_calendar.LastMinedCreditHash());

    ASSERT_THAT(node1.calendar, Ne(original_calendar));
    ASSERT_THAT(node1.calendar.calends.size(), Eq(1));
    ASSERT_THAT(node1.calendar.LastMinedCredit().network_state.batch_number, Eq(bad_batch_number - 1));
}



class TwoFlexNetworkNodesConnectedViaPeersAfterOneHasBuiltACalendarWithAFailure :
        public TwoFlexNetworkNodesConnectedViaCommunicatorsAfterOneHasBuiltACalendarWithAFailure
{
public:
    TestPeerWithNetworkNode peer1, peer2;

    virtual void SetUp()
    {
        TwoFlexNetworkNodesWithCommunicators::SetUp();
        SetMiningPreferences(node1);
        SetMiningPreferences(node2);

        BuildACalendarWithAFailure(node1);
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
        node1.StopCommunicator();
        node2.StopCommunicator();

        MilliSleep(150);
    }
};

TEST_F(TwoFlexNetworkNodesConnectedViaPeersAfterOneHasBuiltACalendarWithAFailure,
       SynchronizeToTheLastGoodMinedCreditIfTheFailureIsDetected)
{
    Calendar original_calendar = node1.calendar;
    node2.data_message_handler->calendar_scrutiny_time = 100 * CALENDAR_SCRUTINY_TIME; // error will be found
    node2.data_message_handler->RequestTips(); // node2 should inform node1 of the error

    WaitForCalendarSwitch(node1, original_calendar.LastMinedCreditHash());

    ASSERT_THAT(node1.calendar, Ne(original_calendar));
    ASSERT_THAT(node1.calendar.calends.size(), Eq(1));
    ASSERT_THAT(node1.calendar.LastMinedCredit().network_state.batch_number, Eq(bad_batch_number - 1));
}


class TwoFlexNetworkNodesWhoHaveDetectedAFailureInACalendar :
        public TwoFlexNetworkNodesConnectedViaPeersAfterOneHasBuiltACalendarWithAFailure
{
public:
    Calendar original_calendar;

    virtual void SetUp()
    {
        TwoFlexNetworkNodesConnectedViaPeersAfterOneHasBuiltACalendarWithAFailure::SetUp();

        original_calendar = node1.calendar;
        node2.data_message_handler->calendar_scrutiny_time = 100 * CALENDAR_SCRUTINY_TIME; // error will be found
        node2.data_message_handler->RequestTips(); // node2 should inform node1 of the error

        WaitForCalendarSwitch(node1, original_calendar.LastMinedCreditHash());
    }

};

TEST_F(TwoFlexNetworkNodesWhoHaveDetectedAFailureInACalendar,
       SendCalendarFailureMessagesInResponseToCalendarsWhichContainTheBadCalend)
{
    auto calendar_with_failure = original_calendar;
    calendar_with_failure.RemoveLast(node1.credit_system);

    CalendarRequestMessage request(calendar_with_failure.LastMinedCredit());
    node2.msgdata[request.GetHash160()]["is_calendar_request"] = true;
    CalendarMessage calendar_message(request, node1.credit_system);
    node2.data_message_handler->HandleMessage(GetDataStream("data", calendar_message), &peer1);
    MilliSleep(3000);
    CalendarFailureDetails details = node2.data_message_handler->GetCalendarFailureDetails(calendar_with_failure);

    ASSERT_TRUE(peer1.HasBeenInformedAbout("data", "calendar_failure", CalendarFailureMessage(details)));
}
