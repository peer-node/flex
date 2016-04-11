#ifndef FLEX_NET_SYNC_H
#define FLEX_NET_SYNC_H

#include "net_cnode.h"

int64_t NodeSyncScore(const CNode *pnode);
void StartSync(const vector<CNode*> &vNodes);

#endif //FLEX_NET_SYNC_H
