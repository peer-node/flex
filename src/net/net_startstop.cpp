#include <src/base/util_init.h>
#include "net.h"
#include "net_cnode.h"
#include "net_services.h"

#define DUMP_ADDRESSES_INTERVAL 900



void Network::Discover(boost::thread_group& threadGroup)
{
    if (!fDiscover)
        return;

#ifdef WIN32
    // Get local host IP
    char pszHostName[1000] = "";
    if (gethostname(pszHostName, sizeof(pszHostName)) != SOCKET_ERROR)
    {
        vector<CNetAddr> vaddr;
        if (LookupHost(pszHostName, vaddr))
        {
            BOOST_FOREACH (const CNetAddr &addr, vaddr)
            {
                AddLocal(addr, LOCAL_IF);
            }
        }
    }
#else
    // Get local host ip
    struct ifaddrs* myaddrs;
    if (getifaddrs(&myaddrs) == 0)
    {
        for (struct ifaddrs* ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL) continue;
            if ((ifa->ifa_flags & IFF_UP) == 0) continue;
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;
            if (strcmp(ifa->ifa_name, "lo0") == 0) continue;
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                struct sockaddr_in* s4 = (struct sockaddr_in*)(ifa->ifa_addr);
                CNetAddr addr(s4->sin_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("IPv4 %s: %s\n", ifa->ifa_name, addr.ToString());
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6)
            {
                struct sockaddr_in6* s6 = (struct sockaddr_in6*)(ifa->ifa_addr);
                CNetAddr addr(s6->sin6_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("IPv6 %s: %s\n", ifa->ifa_name, addr.ToString());
            }
        }
        freeifaddrs(myaddrs);
    }
#endif

    // Don't use external IPv4 discovery, when -onlynet="IPv6"
    if (!IsLimited(NET_IPV4))
    {
        boost::thread external_ip_thread(&Network::TraceThreadGetMyExternalIP, boost::ref(this));
    }

}

void Network::TraceThreadGetMyExternalIP()
{
    RenameThread("get_ext_ip");
    try
    {
        LogPrintf("get_ext_ip thread start\n");
        ThreadGetMyExternalIP();
        LogPrintf("get_ext_ip thread exit\n");
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("%s thread interrupt\n");
        throw;
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "get_ext_ip");
        throw;
    }
    catch (...) {
        PrintExceptionContinue(NULL, "get_ext_ip");
        throw;
    }
}

void Network::StartNode(boost::thread_group& threadGroup)
{
    if (semOutbound == NULL) {
        // initialize semaphore
        int nMaxOutbound = min(MAX_OUTBOUND_CONNECTIONS, nMaxConnections);
        semOutbound = new CSemaphore(nMaxOutbound);
    }

    if (pnodeLocalHost == NULL)
        pnodeLocalHost = new CNode(*this, INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), nLocalServices));

    Discover(threadGroup);

    //
    // Start threads
    //

    if (!GetBoolArg("-dnsseed", true))
        LogPrintf("DNS seeding disabled\n");
    else
    {
        boost::thread dns_address_thread(&Network::ThreadDNSAddressSeed, boost::ref(this));
        LogPrintf("DNS seeding enabled\n");
    }

#ifdef USE_UPNP
    // Map ports with UPnP
    MapPort(GetBoolArg("-upnp", USE_UPNP));
#endif

    // Send and receive from sockets, accept connections
    boost::thread socket_handler_thread(&Network::ThreadSocketHandler, boost::ref(this));

    // Initiate outbound connections from -addnode
    boost::thread open_added_connections_thread(&Network::ThreadOpenAddedConnections, boost::ref(this));

    // Initiate outbound connections
    boost::thread open_connections_thread(&Network::ThreadOpenConnections, boost::ref(this));

    // Process messages
    boost::thread message_handler_thread(&Network::ThreadMessageHandler, boost::ref(this));

    // Dump network addresses
    boost::thread dump_addresses_thread(&Network::LoopForeverDumpAddresses, boost::ref(this));

}

void Network::LoopForeverDumpAddresses()
{
    RenameThread("dump_addresses");
    LogPrintf("dump_addresses thread start\n");
    try
    {
        while (1)
        {
            MilliSleep(DUMP_ADDRESSES_INTERVAL * 1000);
            DumpAddresses();
        }
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("dump_addresses thread stop\n", "dump_addresses");
        throw;
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "dump_addresses");
        throw;
    }
    catch (...) {
        PrintExceptionContinue(NULL, "dump_addresses");
        throw;
    }
}
bool Network::StopNode()
{
    LogPrintf("StopNode()\n");
    MapPort(false);
    if (semOutbound)
        for (int i = 0; i < MAX_OUTBOUND_CONNECTIONS; i++)
            semOutbound->post();
    MilliSleep(50);
    DumpAddresses();

    return true;
}
