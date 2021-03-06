#include "gmock/gmock.h"
#include <src/net/net.h>
#include <src/teleportnode/main.h>
#include <test/teleport_tests/node/TeleportNetworkNode.h>
#include "Communicator.h"

using namespace ::testing;
using namespace std;


TEST(ACommunicator, ListensToAnotherCommunicator)
{
    MilliSleep(100);
    uint64_t port1 = 10000 + GetRand(10000);
    uint64_t port2 = port1 + 1;
    Communicator communicator1("1", port1);
    Communicator communicator2("2", port2);

    ASSERT_TRUE(communicator1.StartListening());
    ASSERT_TRUE(communicator2.StartListening());

    communicator1.network.AddNode("127.0.0.1:" + PrintToString(port2));

    MilliSleep(800);

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
        uint64_t port1 = 10000 + GetRand(10000);
        uint64_t port2 = port1 + 1;
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


class TwoConnectedCommunicatorsWithTeleportNetworkNodes : public TwoConnectedCommunicators
{
public:
    TeleportNetworkNode *teleport_network_node1;
    TeleportNetworkNode *teleport_network_node2;

    virtual void SetUp()
    {
        teleport_network_node1 = new TeleportNetworkNode();
        teleport_network_node2 = new TeleportNetworkNode();
        TwoConnectedCommunicators::SetUp();
        communicator1->main_node.SetTeleportNetworkNode(*teleport_network_node1);
        communicator2->main_node.SetTeleportNetworkNode(*teleport_network_node2);

        MilliSleep(100);

        communicator2->Node(0)->WaitForVersion();
        communicator1->Node(0)->WaitForVersion();
    }

    virtual void TearDown()
    {
        MilliSleep(50);
        TwoConnectedCommunicators::TearDown();
        delete teleport_network_node1;
        delete teleport_network_node2;
    }
};


TEST_F(TwoConnectedCommunicatorsWithTeleportNetworkNodes, AreConnected)
{
    MilliSleep(100);
    ASSERT_THAT(communicator1->network.vNodes.size(), Eq(1));
    ASSERT_THAT(communicator2->network.vNodes.size(), Eq(1));
}

TEST_F(TwoConnectedCommunicatorsWithTeleportNetworkNodes, SendAndReceiveMessages)
{
    SignedTransaction tx;

    communicator2->Node(0)->PushMessage("credit", "tx", tx);

    MilliSleep(100);

    ASSERT_THAT(teleport_network_node1->msgdata[tx.GetHash160()].HasProperty("tx"), Eq(true));
}

