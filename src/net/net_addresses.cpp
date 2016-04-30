#include "net.h"
#include "net_services.h"
#include "net_caddrdb.h"



void Network::AddressCurrentlyConnected(const CService& addr)
{
    addrman.Connected(addr);
}

void Network::ThreadDNSAddressSeed()
{
    const vector<CDNSSeedData> &vSeeds = Params().DNSSeeds();
    int found = 0;

    boost::this_thread::interruption_point();
    LogPrintf("Loading addresses from DNS seeds (could take a while)\n");

    BOOST_FOREACH(const CDNSSeedData &seed, vSeeds) {

        boost::this_thread::interruption_point();

        if (HaveNameProxy()) {
            AddOneShot(seed.host);
        } else {
            vector<CNetAddr> vIPs;
            vector<CAddress> vAdd;
            if (LookupHost(seed.host.c_str(), vIPs))
            {
                BOOST_FOREACH(CNetAddr& ip, vIPs)
                            {
                                int nOneDay = 24*3600;
                                CAddress addr = CAddress(CService(ip, Params().GetDefaultPort()));
                                addr.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay); // use a random age between 3 and 7 days old
                                vAdd.push_back(addr);
                                found++;
                            }
            }
            addrman.Add(vAdd, CNetAddr(seed.name, true));
        }
    }

    LogPrintf("%d addresses found from DNS seeds\n", found);
}

void Network::DumpAddresses()
{
    int64_t nStart = GetTimeMillis();

    CAddrDB adb(network_name);
    adb.Write(addrman);

//    LogPrint("net", "Flushed %d addresses to peers.dat  %dms\n",
//             addrman.size(), GetTimeMillis() - nStart);
}
