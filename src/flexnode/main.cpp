#include "main.h"
#include "net/net.h"

#include "log.h"
#define LOG_CATEGORY "main.cpp"


void MainRoutine::SetNetwork(Network& network_)
{
    network = &network_;
    RegisterNodeSignals(network->GetNodeSignals());
}

void MainRoutine::SetFlexNode(MainFlexNode& flexnode_)
{
    flexnode = &flexnode_;
}

class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup() {
        // block headers
        
    }
} instance_of_cmaincleanup;

