#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#if defined(HAVE_CONFIG_H)
#include "flex-config.h"
#endif


#include "crypto/uint256.h"
#include "net/net.h"
#include "MainFlexNode.h"

#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#define BLOCK_DOWNLOAD_TIMEOUT 20 // seconds

class CInv;

#ifdef USE_UPNP
static const int fHaveUPnP = true;
#else
static const int fHaveUPnP = false;
#endif

#define MAX_BLOCKS_IN_TRANSIT_PER_PEER 5000

/** "reject" message codes **/
static const unsigned char REJECT_MALFORMED = 0x01;
static const unsigned char REJECT_INVALID = 0x10;
static const unsigned char REJECT_OBSOLETE = 0x11;
static const unsigned char REJECT_DUPLICATE = 0x12;

struct CNodeStateStats
{
    int nMisbehavior;
};


struct CMainSignals
{
    // Notifies listeners about an inventory item being seen on the network.
    boost::signals2::signal<void (const uint256 &)> Inventory;
    // Tells listeners to broadcast their data.
    boost::signals2::signal<void ()> Broadcast;
};

struct QueuedBlock
{
    uint256 hash;
    int64_t nTime;  // Time of "getdata" request in microseconds.
    int nQueuedBefore;  // Number of blocks in flight at the time of request.
};

struct CNodeState
{
    // Accumulated misbehaviour score for this peer.
    int nMisbehavior;
    // Whether this peer should be disconnected and banned.
    bool fShouldBan;
    // String name of this peer (debugging/logging purposes).
    std::string name;
    // List of asynchronously-determined block rejections to notify this peer about.
    int nBlocksInFlight;
    std::list<QueuedBlock> vBlocksInFlight;
    std::list<uint256> vBlocksToDownload;
    int nBlocksToDownload;
    int64_t nLastBlockReceive;
    int64_t nLastBlockProcess;

    CNodeState() {
        nMisbehavior = 0;
        fShouldBan = false;
        nBlocksToDownload = 0;
        nBlocksInFlight = 0;
        nLastBlockReceive = 0;
        nLastBlockProcess = 0;
    }
};

class Network;

class MainRoutine
{
public:
    CMainSignals g_signals;
    std::string node_name;
    Network *network;
    MainFlexNode *flexnode = NULL;

    CCriticalSection cs_main;

    std::map<uint256, std::pair<NodeId, std::list<QueuedBlock>::iterator> > mapBlocksInFlight;
    std::map<uint256, std::pair<NodeId, std::list<uint256>::iterator> > mapBlocksToDownload;

    // Map maintaining per-node state. Requires cs_main.
    std::map<NodeId, CNodeState> mapNodeState;

    CNodeState *StateOfNode(NodeId pnode);

    void SetNetwork(Network& network_);

    void SetFlexNode(MainFlexNode& flexnode_);

    bool AddBlockToQueue(NodeId nodeid, const uint256 &hash);

    void MarkBlockAsReceived(const uint256 &hash, NodeId nodeFrom = -1);

    void MarkBlockAsInFlight(NodeId nodeid, const uint256 &hash);

    void InitializeNode(NodeId nodeid, const CNode *pnode);

    void FinalizeNode(NodeId nodeid);

    bool IsInitialBlockDownload();

    bool AlreadyHave(const CInv& inv);

    void RegisterNodeSignals(CNodeSignals& nodeSignals);

    void UnregisterNodeSignals(CNodeSignals& nodeSignals);

    bool ProcessMessages(CNode* pfrom);

    bool ProcessMessage(CNode* pfrom, std::string strCommand, CDataStream& vRecv);

    bool SendMessages(CNode* pto);

    bool AbortNode(const std::string &message);

    bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);

    void Misbehaving(NodeId nodeid, int howmuch);

    void ProcessGetData(CNode *pfrom);
};

#endif
