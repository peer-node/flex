#include "../../test/flex_tests/flex_data/TestData.h"
#include <src/base/util_data.h>
#include <boost/filesystem/operations.hpp>
#include <src/base/util_init.h>
#include <src/net/net_startstop.h>
#include "main.h"

#include "log.h"
#define LOG_CATEGORY "main_init.cpp"

volatile bool fRequestShutdown = false;

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

    RenameThread("flex-shutoff");
    StopNode();

    UnregisterNodeSignals(GetNodeSignals());
    {
        LOCK(cs_main);
    }

    boost::filesystem::remove(GetPidFile());

    LogPrintf("Shutdown : done\n");
}
