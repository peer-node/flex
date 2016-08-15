#include "../../test/flex_tests/flex_data/TestData.h"

#include "flexnode/main.h"
#include "net/net_cnode.h"

#include "flexnode/MockInit.h"

using namespace std;
using namespace boost;

#include "log.h"
#define LOG_CATEGORY "main_signals.cpp"


// Requires cs_main.
CNodeState *MainRoutine::StateOfNode(NodeId pnode)
{
    map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode);
    if (it == mapNodeState.end())
        return NULL;
    return &it->second;
}

void MainRoutine::InitializeNode(NodeId nodeid, const CNode *pnode)
{
    LOCK(cs_main);
    CNodeState &state = mapNodeState.insert(std::make_pair(nodeid, CNodeState())).first->second;
    state.name = pnode->addrName;
}

void MainRoutine::FinalizeNode(NodeId nodeid) {
    LOCK(cs_main);
    CNodeState *state = StateOfNode(nodeid);

    if (state == NULL)
    {
        return;
    }
    BOOST_FOREACH(const QueuedBlock& entry, state->vBlocksInFlight)
    {
        mapBlocksInFlight.erase(entry.hash);
    }

    BOOST_FOREACH(const uint256& hash, state->vBlocksToDownload)
        mapBlocksToDownload.erase(hash);

    mapNodeState.erase(nodeid);
}

// Requires cs_main.
void MainRoutine::MarkBlockAsReceived(const uint256 &hash, NodeId nodeFrom)
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
bool MainRoutine::AddBlockToQueue(NodeId nodeid, const uint256 &hash) {
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
void MainRoutine::MarkBlockAsInFlight(NodeId nodeid, const uint256 &hash)
{
    CNodeState *state = StateOfNode(nodeid);
    assert(state != NULL);

    // Make sure it's not listed somewhere already.
    MarkBlockAsReceived(hash);

    QueuedBlock newentry = {hash, (int64_t) GetTimeMicros(), state->nBlocksInFlight};
    if (state->nBlocksInFlight == 0)
        state->nLastBlockReceive = newentry.nTime; // Reset when a first request is sent.
    list<QueuedBlock>::iterator it = state->vBlocksInFlight.insert(state->vBlocksInFlight.end(), newentry);
    state->nBlocksInFlight++;
    mapBlocksInFlight[hash] = std::make_pair(nodeid, it);
}

bool MainRoutine::GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats)
{
    LOCK(cs_main);
    CNodeState *state = StateOfNode(nodeid);
    if (state == NULL)
        return false;
    stats.nMisbehavior = state->nMisbehavior;
    return true;
}

void MainRoutine::RegisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.ProcessMessages.connect(boost::bind(&MainRoutine::ProcessMessages, this, _1));
    nodeSignals.SendMessages.connect(boost::bind(&MainRoutine::SendMessages, this, _1));
    nodeSignals.InitializeNode.connect(boost::bind(&MainRoutine::InitializeNode, this, _1, _2));
    nodeSignals.FinalizeNode.connect(boost::bind(&MainRoutine::FinalizeNode, this, _1));
}

void MainRoutine::UnregisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.ProcessMessages.disconnect(&MainRoutine::ProcessMessages);
    nodeSignals.SendMessages.disconnect(&MainRoutine::SendMessages);
    nodeSignals.InitializeNode.disconnect(&MainRoutine::InitializeNode);
    nodeSignals.FinalizeNode.disconnect(&MainRoutine::FinalizeNode);
}

// Requires cs_main.
void MainRoutine::Misbehaving(NodeId pnode, int howmuch)
{
    if (howmuch == 0)
        return;

    CNodeState *state = StateOfNode(pnode);
    if (state == NULL)
        return;

    state->nMisbehavior += howmuch;
    if (state->nMisbehavior >= 100)
    {
        log_ << "Misbehaving: " << state->name
             << " (" << state->nMisbehavior << " -> "
             << state->nMisbehavior << ") BAN THRESHOLD EXCEEDED\n";
        state->fShouldBan = true;
    }
    else
        log_ << "Misbehaving: " << state->name
             << " (" << state->nMisbehavior << ")\n";
}

bool MainRoutine::AbortNode(const std::string &strMessage)
{
    strMiscWarning = strMessage;
    log_ << "*** " << strMessage << "\n";
    StartShutdown();
    return false;
}

