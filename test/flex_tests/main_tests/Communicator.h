#ifndef FLEX_COMMUNICATOR_H
#define FLEX_COMMUNICATOR_H

#include "net/net.h"
#include "flexnode/main.h"
#include "net/net_cnode.h"

class Communicator
{
public:
    Network network;
    MainRoutine main_node;

    Communicator(std::string network_name, unsigned short port)
    {
        MilliSleep(100);
        network.SetName(network_name);
        network.SetListenPort(port);
        main_node.node_name = network_name;
    }

    bool StartListening()
    {
        network.InitializeNode();
        std::string str_error;
        bool result = network.StartListening(str_error);
        main_node.SetNetwork(network);
        return result;
    }

    void StopListening()
    {
        network.StopNode();
        main_node.UnregisterNodeSignals(network.GetNodeSignals());
    }

    CNode *Node(uint32_t i)
    {
        return network.vNodes[i];
    }
};


#endif //FLEX_COMMUNICATOR_H
