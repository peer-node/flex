#include "gmock/gmock.h"
#include "Communicator.h"
#include "CreditMessageHandler.h"

using namespace ::testing;
using namespace std;


TEST(ACreditMessageHandler, CountsTheNumberOfMessagesReceived)
{
    CreditMessageHandler credit_message_handler;
    ASSERT_THAT(credit_message_handler.messages_received, Eq(0));
    CDataStream datastream(SER_NETWORK, CLIENT_VERSION);
    CNode *peer = NULL;
    credit_message_handler.HandleMessage("test_message", datastream, peer);
    ASSERT_THAT(credit_message_handler.messages_received, Eq(1));
}

TEST(ACreditMessageHandler, SetsAFlexNode)
{
    MainFlexNode flexnode;
    CreditMessageHandler credit_message_handler;
    credit_message_handler.SetFlexNode(flexnode);
    ASSERT_THAT(credit_message_handler.flexnode, Eq(&flexnode));
}

