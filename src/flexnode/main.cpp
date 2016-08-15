#include <test/flex_tests/node/FlexNetworkNode.h>
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

void MainRoutine::SetFlexNetworkNode(FlexNetworkNode &flex_network_node_)
{
    flex_network_node = &flex_network_node_;
}

class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup() {
        // block headers
        
    }
} instance_of_cmaincleanup;

