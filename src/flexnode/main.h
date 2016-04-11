#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#include "crypto/point.h"

#if defined(HAVE_CONFIG_H)
#include "flex-config.h"
#endif

#include "crypto/bignum.h"
#include "base/chainparams.h"
#include "net/net.h"
#include "base/sync.h"
#include "crypto/uint256.h"

#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>
#include <src/net/net_signals.h>

#define BLOCK_DOWNLOAD_TIMEOUT 20 // seconds

class CInv;

#ifdef USE_UPNP
static const int fHaveUPnP = true;
#else
static const int fHaveUPnP = false;
#endif

extern CCriticalSection cs_main;
extern const std::string strMessageMagic;
extern int64_t nTimeBestReceived;
extern bool fImporting;
extern bool fReindex;
extern bool fBenchmark;

struct CNodeStateStats;
#define MAX_BLOCKS_IN_TRANSIT_PER_PEER 5000

/** "reject" message codes **/
static const unsigned char REJECT_MALFORMED = 0x01;
static const unsigned char REJECT_INVALID = 0x10;
static const unsigned char REJECT_OBSOLETE = 0x11;
static const unsigned char REJECT_DUPLICATE = 0x12;
static const unsigned char REJECT_NONSTANDARD = 0x40;
static const unsigned char REJECT_DUST = 0x41;
static const unsigned char REJECT_INSUFFICIENTFEE = 0x42;
static const unsigned char REJECT_CHECKPOINT = 0x43;

/** Register with a network node to receive its signals */
void RegisterNodeSignals(CNodeSignals& nodeSignals);
/** Unregister a network node */
void UnregisterNodeSignals(CNodeSignals& nodeSignals);


/** Process protocol messages received from a given node */
bool ProcessMessages(CNode* pfrom);
/** Send queued protocol messages to be sent to a give node */
bool SendMessages(CNode* pto, bool fSendTrickle);
/** Format a string that describes several potential problems detected by the core */
std::string GetWarnings(std::string strFor);

/** Abort with a message */
bool AbortNode(const std::string &msg);
/** Get statistics from node state */
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);
/** Increase a node's misbehavior score. */
void Misbehaving(NodeId nodeid, int howmuch);

struct CNodeStateStats {
    int nMisbehavior;
};

//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions
//

// These functions dispatch to one or all registered wallets


struct CMainSignals {
    // Notifies listeners about an inventory item being seen on the network.
    boost::signals2::signal<void (const uint256 &)> Inventory;
    // Tells listeners to broadcast their data.
    boost::signals2::signal<void ()> Broadcast;
};

extern CMainSignals g_signals;

struct QueuedBlock {
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
    list<QueuedBlock> vBlocksInFlight;
    list<uint256> vBlocksToDownload;
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

CNodeState *StateOfNode(NodeId pnode);

bool AddBlockToQueue(NodeId nodeid, const uint256 &hash);
void MarkBlockAsReceived(const uint256 &hash, NodeId nodeFrom = -1);
void MarkBlockAsInFlight(NodeId nodeid, const uint256 &hash);
void InitializeNode(NodeId nodeid, const CNode *pnode);
void FinalizeNode(NodeId nodeid);



bool IsInitialBlockDownload();
bool AlreadyHave(const CInv& inv);

#endif
