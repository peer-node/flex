#include "net.h"
#include "net_cnode.h"

// for now, use a very simple selection metric: the node from which we received
// most recently
int64_t Network::NodeSyncScore(const CNode *pnode)
{
    return pnode->nLastRecv;
}

void Network::StartSync(const vector<CNode*> &vNodes)
{
    CNode *pnodeNewSync = NULL;
    int64_t nBestScore = 0;

    int nBestHeight = 0;//node_signals.GetHeight().get_value_or(0);

    // Iterate over all nodes
    BOOST_FOREACH(CNode* pnode, vNodes) {
        // check preconditions for allowing a sync
        if (!pnode->fClient && !pnode->fOneShot &&
            !pnode->fDisconnect && pnode->fSuccessfullyConnected &&
            (pnode->nStartingHeight > (nBestHeight - 144)) &&
            (pnode->nVersion < NOBLKS_VERSION_START || pnode->nVersion >= NOBLKS_VERSION_END)) {
            // if ok, compare node's score with the best so far
            int64_t nScore = NodeSyncScore(pnode);
            if (pnodeNewSync == NULL || nScore > nBestScore) {
                pnodeNewSync = pnode;
                nBestScore = nScore;
            }
        }
    }
    // if a new sync candidate was found, start sync!
    if (pnodeNewSync) {
        pnodeNewSync->fStartSync = true;
        pnodeSync = pnodeNewSync;
    }
}
