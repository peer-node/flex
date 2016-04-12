// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_H
#define BITCOIN_NET_H

#include "credits/credits.h"
#include "base/bloom.h"
#include "compat.h"
#include "crypto/hash.h"
#include "base/limitedmap.h"
#include "base/mruset.h"
#include "net/netbase.h"
#include "base/protocol.h"
#include "base/sync.h"
#include "crypto/uint256.h"
#include "base/util.h"

#include <deque>
#include <stdint.h>

#ifndef WIN32
#include <arpa/inet.h>
#endif

#include <boost/foreach.hpp>
#include <openssl/rand.h>
#include <src/base/util_args.h>
#include <src/base/util_time.h>
#include <src/base/util_format.h>
#include <src/base/util_rand.h>

#include "net/addrman.h"

#include <boost/signals2/signal.hpp>

#define MAX_OUTBOUND_CONNECTIONS 8

class CNode;

namespace boost {
    class thread_group;
}

/** The maximum number of entries in an 'inv' protocol message */
static const unsigned int MAX_INV_SZ = 50000;

inline unsigned int ReceiveFloodSize() { return 1000*GetArg("-maxreceivebuffer", 5*1000); }
inline unsigned int SendBufferSize() { return 1000*GetArg("-maxsendbuffer", 1*1000); }

void MapPort(bool fUseUPnP);

typedef int NodeId;


struct LocalServiceInfo {
    int nScore;
    int nPort;
};

class CNodeStats
{
public:
    NodeId nodeid;
    uint64_t nServices;
    int64_t nLastSend;
    int64_t nLastRecv;
    int64_t nTimeConnected;
    std::string addrName;
    int nVersion;
    std::string cleanSubVer;
    bool fInbound;
    int nStartingHeight;
    uint64_t nSendBytes;
    uint64_t nRecvBytes;
    bool fSyncNode;
    double dPingTime;
    double dPingWait;
    std::string addrLocal;
};


// Signals for message handling
struct CNodeSignals
{
    boost::signals2::signal<int ()> GetHeight;
    boost::signals2::signal<bool (CNode*)> ProcessMessages;
    boost::signals2::signal<bool (CNode*, bool)> SendMessages;
    boost::signals2::signal<void (NodeId, const CNode*)> InitializeNode;
    boost::signals2::signal<void (NodeId)> FinalizeNode;
};


class Network
{
public:
    CSemaphore *semOutbound = NULL;
    bool fDiscover = true;
    uint64_t nLocalServices = NODE_NETWORK;
    CCriticalSection cs_mapLocalHost;
    map<CNetAddr, LocalServiceInfo> mapLocalHost;

    CNode* pnodeLocalHost = NULL;
    CNode* pnodeSync = NULL;
    uint64_t nLocalHostNonce = 0;

    CAddrMan addrman;
    int nMaxConnections = 125;

    vector<CNode*> vNodes;
    CCriticalSection cs_vNodes;
    map<CInv, CDataStream> mapRelay;
    deque<pair<int64_t, CInv> > vRelayExpiration;
    CCriticalSection cs_mapRelay;
    limitedmap<CInv, int64_t> mapAlreadyAskedFor;


    vector<std::string> vAddedNodes;
    CCriticalSection cs_vAddedNodes;

    NodeId nLastNodeId = 0;
    CCriticalSection cs_nLastNodeId;

    list<CNode*> vNodesDisconnected;

    deque<string> vOneShots;
    CCriticalSection cs_vOneShots;

    bool vfReachable[NET_MAX] = {};
    bool vfLimited[NET_MAX] = {};

    std::vector<SOCKET> vhListenSocket;

    CNodeSignals node_signals;
    CNodeSignals& GetNodeSignals() { return node_signals; }

    Network()
    {
        mapAlreadyAskedFor = limitedmap<CInv, int64_t>(MAX_INV_SZ);
    }

    ~Network();

    bool AddLocal(const CService &addr, int nScore);

    bool IsReachable(const CNetAddr &addr);

    void ProcessOneShot();

    bool IsLocal(const CService &addr);

    bool SeenLocal(const CService &addr);

    bool IsLimited(const CNetAddr &addr);

    bool IsLimited(NetworkType net);

    void SetLimited(NetworkType net, bool fLimited);

    bool AddLocal(const CNetAddr &addr, int nScore);

    void SetReachable(NetworkType net, bool fFlag = true);

    void AdvertizeLocal();

    bool RecvLine(SOCKET hSocket, string &strLine);

    CAddress GetLocalAddress(const CNetAddr *paddrPeer);

    bool GetLocal(CService &addr, const CNetAddr *paddrPeer);

    unsigned short GetListenPort();

    void AddOneShot(string strDest);

    void ThreadSocketHandler();

    bool BindListenPort(const CService &addrBind, string &strError);

    void SocketSendData(CNode *pnode);

    void Discover(boost::thread_group &threadGroup);

    void StartNode(boost::thread_group &threadGroup);

    bool StopNode();

    void ThreadOpenConnections();

    void ThreadOpenAddedConnections();

    bool OpenNetworkConnection(const CAddress &addrConnect, CSemaphoreGrant *grantOutbound, const char *strDest = NULL,
                               bool fOneShot = false);

    void ThreadMessageHandler();

    bool GetMyExternalIP2(const CService &addrConnect, const char *pszGet, const char *pszKeyword, CNetAddr &ipRet);

    bool GetMyExternalIP(CNetAddr &ipRet);

    void ThreadGetMyExternalIP();

    CNode *ConnectNode(CAddress addrConnect, const char *pszDest);

    CNode *FindNode(const CService &addr);

    CNode *FindNode(const CNetAddr &ip);

    CNode *FindNode(string addrName);

    void AddressCurrentlyConnected(const CService &addr);

    void ThreadDNSAddressSeed();

    void DumpAddresses();

    int64_t NodeSyncScore(const CNode *pnode);

    void StartSync(const vector<CNode *> &vNodes);

    void RelayTransaction(const SignedTransaction &tx);

    void RelayTransaction(const SignedTransaction &tx, const uint256 &hash);

    void RelayTransaction(const SignedTransaction &tx, const uint256 &hash, const CDataStream &ss);

    void TellNodeAboutTransaction(CNode *pnode, const SignedTransaction &tx);

    void RelayMinedCredit(const MinedCreditMessage &message);

    void RelayMinedCredit(const MinedCreditMessage &message, const uint256 &hash, const CDataStream &ss);

    void RelayMessage(const CDataStream &ss, int type);

    void RelayTradeMessage(const CDataStream &ss);

    void RelayDepositMessage(const CDataStream &ss);

    void RelayRelayMessage(const CDataStream &ss);

    template <typename Callable>
    void TraceThread(const char *name, Callable func);

    void TraceThreadGetMyExternalIP();

    void LoopForeverDumpAddresses();
};

extern Network network;

#endif
