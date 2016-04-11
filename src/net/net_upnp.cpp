#include "net.h"


#ifdef USE_UPNP
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#endif


#ifdef USE_UPNP
void ThreadMapPort()
{
    std::string port = strprintf("%u", GetListenPort());
    const char * multicastif = 0;
    const char * minissdpdpath = 0;
    struct UPNPDev * devlist = 0;
    char lanaddr[64];

#ifndef UPNPDISCOVER_SUCCESS
    /* miniupnpc 1.5 */
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0);
#else
    /* miniupnpc 1.6 */
    int error = 0;
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1)
    {
        if (fDiscover) {
            char externalIPAddress[40];
            r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress);
            if(r != UPNPCOMMAND_SUCCESS)
                LogPrintf("UPnP: GetExternalIPAddress() returned %d\n", r);
            else
            {
                if(externalIPAddress[0])
                {
                    LogPrintf("UPnP: ExternalIPAddress = %s\n", externalIPAddress);
                    AddLocal(CNetAddr(externalIPAddress), LOCAL_UPNP);
                }
                else
                    LogPrintf("UPnP: GetExternalIPAddress failed.\n");
            }
        }

        string strDesc = "Bitcoin " + FormatFullVersion();

        try {
            while (true) {
#ifndef UPNPDISCOVER_SUCCESS
                /* miniupnpc 1.5 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0);
#else
                /* miniupnpc 1.6 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0, "0");
#endif

                if(r!=UPNPCOMMAND_SUCCESS)
                    LogPrintf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
                        port, port, lanaddr, r, strupnperror(r));
                else
                    LogPrintf("UPnP Port Mapping successful.\n");;

                MilliSleep(20*60*1000); // Refresh every 20 minutes
            }
        }
        catch (boost::thread_interrupted)
        {
            r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP", 0);
            LogPrintf("UPNP_DeletePortMapping() returned : %d\n", r);
            freeUPNPDevlist(devlist); devlist = 0;
            FreeUPNPUrls(&urls);
            throw;
        }
    } else {
        LogPrintf("No valid UPnP IGDs found\n");
        freeUPNPDevlist(devlist); devlist = 0;
        if (r != 0)
            FreeUPNPUrls(&urls);
    }
}

void MapPort(bool fUseUPnP)
{
    static boost::thread* upnp_thread = NULL;

    if (fUseUPnP)
    {
        if (upnp_thread) {
            upnp_thread->interrupt();
            upnp_thread->join();
            delete upnp_thread;
        }
        upnp_thread = new boost::thread(boost::bind(&TraceThread<void (*)()>, "upnp", &ThreadMapPort));
    }
    else if (upnp_thread) {
        upnp_thread->interrupt();
        upnp_thread->join();
        delete upnp_thread;
        upnp_thread = NULL;
    }
}

#else
void MapPort(bool)
{
    // Intentionally left blank.
}
#endif
