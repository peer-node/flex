#include "gmock/gmock.h"
#include "Calendar.h"
#include "FlexNetworkNode.h"
#include "TestPeer.h"

using namespace ::testing;
using namespace std;

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
    ASSERT_THAT(flex_network_node.Balance(), Eq(1.0));
}

TEST_F(AFlexNetworkNode, RemembersMinedCreditMessagesWithoutProofsOfWorkThatItGenerates)
{
    MinedCreditMessage msg = flex_network_node.GenerateMinedCreditMessageWithoutProofOfWork();
    uint256 credit_hash = msg.mined_credit.GetHash();
    MinedCreditMessage remembered_msg = flex_network_node.RecallGeneratedMinedCreditMessage(credit_hash);
    ASSERT_THAT(remembered_msg, Eq(msg));
}
