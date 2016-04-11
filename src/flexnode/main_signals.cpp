#include "../../test/flex_tests/flex_data/TestData.h"

#include "flexnode/main.h"
#include "net/net_cnode.h"

#include "flexnode/MockInit.h"

using namespace std;
using namespace boost;

#include "log.h"
#define LOG_CATEGORY "main_signals.cpp"



//
// Global state
//

CCriticalSection cs_main;

map<uint256, pair<NodeId, list<QueuedBlock>::iterator> > mapBlocksInFlight;
map<uint256, pair<NodeId, list<uint256>::iterator> > mapBlocksToDownload;


//////////////////////////////////////////////////////////////////////////////
//
// Registration of network node signals.
//
//

// Maintain validation-specific state about nodes, protected by cs_main, instead
// by CNode's own locks. This simplifies asynchronous operation, where
// processing of incoming data is done after the ProcessMessage call returns,
// and we're no longer holding the node's locks.

// Map maintaining per-node state. Requires cs_main.
map<NodeId, CNodeState> mapNodeState;

// Requires cs_main.
CNodeState *StateOfNode(NodeId pnode) {
    map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode);
    if (it == mapNodeState.end())
        return NULL;
    return &it->second;
}


void InitializeNode(NodeId nodeid, const CNode *pnode) {
    LOCK(cs_main);
    CNodeState &state = mapNodeState.insert(std::make_pair(nodeid, CNodeState())).first->second;
    state.name = pnode->addrName;
}

void FinalizeNode(NodeId nodeid) {
    LOCK(cs_main);
    CNodeState *state = StateOfNode(nodeid);

    BOOST_FOREACH(const QueuedBlock& entry, state->vBlocksInFlight)
                    mapBlocksInFlight.erase(entry.hash);
    BOOST_FOREACH(const uint256& hash, state->vBlocksToDownload)
                    mapBlocksToDownload.erase(hash);

    mapNodeState.erase(nodeid);
}

// Requires cs_main.
void MarkBlockAsReceived(const uint256 &hash, NodeId nodeFrom)
{
    log_ << "MarkBlockAsReceived: " << hash << "\n";
    map<uint256, pair<NodeId, list<uint256>::iterator> >::iterator itToDownload = mapBlocksToDownload.find(hash);
    if (itToDownload != mapBlocksToDownload.end())
    {
        CNodeState *state = StateOfNode(itToDownload->second.first);
        state->vBlocksToDownload.erase(itToDownload->second.second);
        state->nBlocksToDownload--;
        log_ << "to download: " << state->nBlocksToDownload << "\n";
        mapBlocksToDownload.erase(itToDownload);
    }

    map<uint256, pair<NodeId, list<QueuedBlock>::iterator> >::iterator itInFlight = mapBlocksInFlight.find(hash);
    if (itInFlight != mapBlocksInFlight.end())
    {
        CNodeState *state = StateOfNode(itInFlight->second.first);
        state->vBlocksInFlight.erase(itInFlight->second.second);
        state->nBlocksInFlight--;
        log_ << "in flight: " << state->nBlocksInFlight << "\n";
        if (itInFlight->second.first == nodeFrom)
            state->nLastBlockReceive = GetTimeMicros();
        mapBlocksInFlight.erase(itInFlight);
    }

}

// Requires cs_main.
bool AddBlockToQueue(NodeId nodeid, const uint256 &hash) {
    if (mapBlocksToDownload.count(hash) || mapBlocksInFlight.count(hash))
        return false;

    CNodeState *state = StateOfNode(nodeid);
    if (state == NULL)
        return false;

    list<uint256>::iterator it = state->vBlocksToDownload.insert(state->vBlocksToDownload.end(), hash);
    state->nBlocksToDownload++;
    if (state->nBlocksToDownload > 5000)
        Misbehaving(nodeid, 10);
    log_ << "AddBlockToQueue: getting " << hash << "\n";
    mapBlocksToDownload[hash] = std::make_pair(nodeid, it);
    return true;
}

// Requires cs_main.
void MarkBlockAsInFlight(NodeId nodeid, const uint256 &hash) {
    CNodeState *state = StateOfNode(nodeid);
    assert(state != NULL);

    // Make sure it's not listed somewhere already.
    MarkBlockAsReceived(hash);

    QueuedBlock newentry = {hash, GetTimeMicros(), state->nBlocksInFlight};
    if (state->nBlocksInFlight == 0)
        state->nLastBlockReceive = newentry.nTime; // Reset when a first request is sent.
    list<QueuedBlock>::iterator it = state->vBlocksInFlight.insert(state->vBlocksInFlight.end(), newentry);
    state->nBlocksInFlight++;
    mapBlocksInFlight[hash] = std::make_pair(nodeid, it);
}

bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats)
{
    LOCK(cs_main);
    CNodeState *state = StateOfNode(nodeid);
    if (state == NULL)
        return false;
    stats.nMisbehavior = state->nMisbehavior;
    return true;
}

void RegisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.ProcessMessages.connect(&ProcessMessages);
    nodeSignals.SendMessages.connect(&SendMessages);
    nodeSignals.InitializeNode.connect(&InitializeNode);
    nodeSignals.FinalizeNode.connect(&FinalizeNode);
}

void UnregisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.ProcessMessages.disconnect(&ProcessMessages);
    nodeSignals.SendMessages.disconnect(&SendMessages);
    nodeSignals.InitializeNode.disconnect(&InitializeNode);
    nodeSignals.FinalizeNode.disconnect(&FinalizeNode);
}

// Requires cs_main.
void Misbehaving(NodeId pnode, int howmuch)
{
    if (howmuch == 0)
        return;

    CNodeState *state = StateOfNode(pnode);
    if (state == NULL)
        return;

    state->nMisbehavior += howmuch;
    if (state->nMisbehavior >= GetArg("-banscore", 100))
    {
        log_ << "Misbehaving: " << state->name
        << " (" << state->nMisbehavior-howmuch << " -> "
        << state->nMisbehavior << ") BAN THRESHOLD EXCEEDED\n";
        state->fShouldBan = true;
    } else
        log_ << "Misbehaving: " << state->name
        << " (" << state->nMisbehavior-howmuch << ")\n";
}

bool AbortNode(const std::string &strMessage) {
    strMiscWarning = strMessage;
    log_ << "*** " << strMessage << "\n";
    StartShutdown();
    return false;
}

