#include "net/net.h"
#include "net/net_cnode.h"

#include "log.h"
#define LOG_CATEGORY "net.cpp"

#if !defined(HAVE_MSG_NOSIGNAL) && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

Network network;
//
//bool fDiscover = true;
//uint64_t nLocalServices = NODE_NETWORK;
//CCriticalSection cs_mapLocalHost;
//map<CNetAddr, LocalServiceInfo> mapLocalHost;
//
//CNode* pnodeLocalHost = NULL;
//CNode* pnodeSync = NULL;
//uint64_t nLocalHostNonce = 0;
//
//CAddrMan addrman;
//int nMaxConnections = 125;
//
//vector<CNode*> vNodes;
//CCriticalSection cs_vNodes;
//map<CInv, CDataStream> mapRelay;
//deque<pair<int64_t, CInv> > vRelayExpiration;
//CCriticalSection cs_mapRelay;
//limitedmap<CInv, int64_t> mapAlreadyAskedFor(MAX_INV_SZ);
//
//
//vector<std::string> vAddedNodes;
//CCriticalSection cs_vAddedNodes;
//
//NodeId nLastNodeId = 0;
//CCriticalSection cs_nLastNodeId;
//
//list<CNode*> vNodesDisconnected;
