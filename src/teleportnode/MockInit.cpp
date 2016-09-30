#include "../../test/teleport_tests/teleport_data/TestData.h"
#include <src/base/util_data.h>
#include <boost/filesystem/operations.hpp>
#include <src/base/util_init.h>
#include <src/net/net_startstop.h>
#include "main.h"

#include "log.h"
#define LOG_CATEGORY "MockInit.cpp"

volatile bool fRequestShutdown = false;

MainRoutine main_node;


void StartShutdown()
{
    fRequestShutdown = true;
}

bool ShutdownRequested()
{
    return fRequestShutdown;
}

void Shutdown()
{
    LogPrintf("Shutdown : In progress...\n");
    static CCriticalSection cs_Shutdown;
    TRY_LOCK(cs_Shutdown, lockShutdown);
    if (!lockShutdown) return;

    RenameThread("teleport-shutoff");
//    network.StopNode();
//
//    main_node.UnregisterNodeSignals(network.GetNodeSignals());
//    {
//        LOCK(main_node.cs_main);
//    }

    boost::filesystem::remove(GetPidFile());

    LogPrintf("Shutdown : done\n");
}
