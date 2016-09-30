#include <test/teleport_tests/node/TeleportNetworkNode.h>
#include "main.h"
#include "net/net.h"

#include "log.h"
#define LOG_CATEGORY "main.cpp"


void MainRoutine::SetNetwork(Network& network_)
{
    network = &network_;
    RegisterNodeSignals(network->GetNodeSignals());
    network->main_routine = this;
}

void MainRoutine::SetTeleportNetworkNode(TeleportNetworkNode &teleport_network_node_)
{
    teleport_network_node = &teleport_network_node_;
}

class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup() {
        // block headers
        
    }
} instance_of_cmaincleanup;

