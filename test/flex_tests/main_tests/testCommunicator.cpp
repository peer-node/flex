#include "gmock/gmock.h"
#include <src/net/net.h>
#include <src/flexnode/main.h>
#include "Communicator.h"

using namespace ::testing;
using namespace std;


TEST(ACommunicator, ListensToAnotherCommunicator)
{
    MilliSleep(100);
    uint64_t port1 = 10000 + GetRand(10000);
    uint64_t port2 = 10000 + GetRand(10000);
    Communicator communicator1("1", port1);
    Communicator communicator2("2", port2);

    ASSERT_TRUE(communicator1.StartListening());
    ASSERT_TRUE(communicator2.StartListening());

    communicator1.network.AddNode("127.0.0.1:" + PrintToString(port2));

    MilliSleep(80);

    ASSERT_THAT(communicator1.network.vNodes.size(), Eq(1));
    ASSERT_THAT(communicator2.network.vNodes.size(), Eq(1));

    communicator1.StopListening();
    communicator2.StopListening();
}

class TwoConnectedCommunicators : public Test
{
public:
    Communicator *communicator1;
    Communicator *communicator2;

    virtual void SetUp()
    {
        MilliSleep(100);
        uint64_t port1 = 10000 + GetRand(10000);;
        uint64_t port2 = 10000 + GetRand(10000);;
        communicator1 = new Communicator("1", port1);
        communicator2 = new Communicator("2", port2);

        communicator1->StartListening();
        communicator2->StartListening();

        communicator1->network.AddNode("127.0.0.1:" + PrintToString(port2));
    }

    virtual void TearDown()
    {
        communicator1->StopListening();
        communicator2->StopListening();

        delete communicator1;
        delete communicator2;
    }
};


TEST_F(TwoConnectedCommunicators, AreConnected)
{
    MilliSleep(100);
    ASSERT_THAT(communicator1->network.vNodes.size(), Eq(1));
    ASSERT_THAT(communicator2->network.vNodes.size(), Eq(1));
}


class TwoConnectedCommunicatorsWithFlexNodes : public TwoConnectedCommunicators
{
public:
    MainFlexNode *flexnode1;
    MainFlexNode *flexnode2;


    virtual void SetUp()
    {
        flexnode1 = new MainFlexNode();
        flexnode2 = new MainFlexNode();
        TwoConnectedCommunicators::SetUp();
        communicator1->main_node.SetFlexNode(*flexnode1);
        communicator2->main_node.SetFlexNode(*flexnode2);
    }

    virtual void TearDown()
    {
        TwoConnectedCommunicators::TearDown();
        delete flexnode1;
        delete flexnode2;
    }
};


TEST_F(TwoConnectedCommunicatorsWithFlexNodes, AreConnected)
{
    MilliSleep(100);
    ASSERT_THAT(communicator1->network.vNodes.size(), Eq(1));
    ASSERT_THAT(communicator2->network.vNodes.size(), Eq(1));
    MilliSleep(50);
}

TEST_F(TwoConnectedCommunicatorsWithFlexNodes, SendAndReceiveMessages)
{
    MilliSleep(100);
    ASSERT_THAT(flexnode1->messages_received, Eq(0));

    communicator2->Node(0)->WaitForVersion();
    communicator2->Node(0)->PushMessage("blahblah", "blah");

    MilliSleep(100);
    ASSERT_THAT(flexnode1->messages_received, Eq(1));
}

