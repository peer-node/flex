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


class CNode;

namespace boost {
    class thread_group;
}

/** The maximum number of entries in an 'inv' protocol message */
static const unsigned int MAX_INV_SZ = 50000;

inline unsigned int ReceiveFloodSize() { return 1000*GetArg("-maxreceivebuffer", 5*1000); }
inline unsigned int SendBufferSize() { return 1000*GetArg("-maxsendbuffer", 1*1000); }


void MapPort(bool fUseUPnP);
unsigned short GetListenPort();
void SocketSendData(CNode *pnode);

extern CNode* pnodeSync;


typedef int NodeId;



extern bool fDiscover;
extern uint64_t nLocalServices;
extern uint64_t nLocalHostNonce;
extern CAddrMan addrman;
extern int nMaxConnections;

extern std::vector<CNode*> vNodes;
extern CCriticalSection cs_vNodes;
extern std::map<CInv, CDataStream> mapRelay;
extern std::deque<std::pair<int64_t, CInv> > vRelayExpiration;
extern CCriticalSection cs_mapRelay;
extern limitedmap<CInv, int64_t> mapAlreadyAskedFor;

extern std::vector<std::string> vAddedNodes;
extern CCriticalSection cs_vAddedNodes;

extern NodeId nLastNodeId;
extern CCriticalSection cs_nLastNodeId;

extern std::list<CNode*> vNodesDisconnected;

struct LocalServiceInfo {
    int nScore;
    int nPort;
};

extern CCriticalSection cs_mapLocalHost;
extern map<CNetAddr, LocalServiceInfo> mapLocalHost;

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


extern CNode* pnodeLocalHost;
static CSemaphore *semOutbound = NULL;

#endif
