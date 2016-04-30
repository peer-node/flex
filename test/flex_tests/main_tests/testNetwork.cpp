#include "gmock/gmock.h"
#include <src/net/net.h>
#include <src/flexnode/main.h>

using namespace ::testing;
using namespace std;

class ANetwork : public Test
{
public:
    Network network;
};


TEST_F(ANetwork, HasASettableListenPort)
{
    uint64_t port = 10000 + GetRand(10000);
    network.SetListenPort(port);
    ASSERT_THAT(network.GetListenPort(), Eq(port));
}

TEST_F(ANetwork, StartsAndStops)
{
    network.InitializeNode();
    MilliSleep(100);
    network.StopNode();
}

TEST_F(ANetwork, ListensOnTheSpecifiedPort)
{
    Network network1;
    uint64_t port1 = 10000 + GetRand(10000);;
    network1.SetName("1");
    network1.SetListenPort(port1);
    network1.InitializeNode(false);

    uint64_t port2 = 10000 + GetRand(10000);;
    Network network2;
    network2.SetName("2");
    network2.SetListenPort(port2);
    network2.InitializeNode(false);

    std::string str_error;

    network1.StartListening(str_error);
    network2.StartListening(str_error);

    MainRoutine main_node1, main_node2;
    main_node1.node_name = "main_node1";
    main_node1.node_name = "main_node2";

    main_node1.SetNetwork(network1);
    main_node2.SetNetwork(network2);

    network1.AddNode("127.0.0.1:" + PrintToString(port2));

    unsigned int nPrevNodeCount = 0;
    network1.SocketHandlerIteration(nPrevNodeCount);
    network2.SocketHandlerIteration(nPrevNodeCount);

    unsigned int i = 0;
    network1.OpenAddedConnectionsIteration(i);
    network2.OpenAddedConnectionsIteration(i);

    int64_t nStart = GetTime();
    network1.OpenConnectionsIteration(nStart);
    network2.OpenConnectionsIteration(nStart);

    ASSERT_THAT(network1.vNodes.size(), Eq(1));
    ASSERT_THAT(network2.vNodes.size(), Eq(1));
}

