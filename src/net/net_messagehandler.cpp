#include <src/base/util_init.h>
#include "net.h"
#include "net_cnode.h"
#include "net_sync.h"

void Network::MessageHandlerIteration()
{
    bool fHaveSyncNode = false;

    vector<CNode*> vNodesCopy;
    {
        boost::this_thread::interruption_point();
        LOCK(cs_vNodes);

        vNodesCopy = vNodes;
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            pnode->AddRef();
            if (pnode == pnodeSync)
                fHaveSyncNode = true;
        }
    }

    if (!fHaveSyncNode)
        StartSync(vNodesCopy);

    // Poll the connected nodes for messages
    CNode* pnodeTrickle = NULL;
    if (!vNodesCopy.empty())
        pnodeTrickle = vNodesCopy[GetRand(vNodesCopy.size())];

    bool fSleep = true;

    BOOST_FOREACH(CNode* pnode, vNodesCopy)
    {
        if (pnode->fDisconnect)
            continue;

        // Receive messages
        {
            TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
            if (lockRecv)
            {
                if (!node_signals.ProcessMessages(pnode))
                {
                    pnode->CloseSocketDisconnect();
                }


                if (pnode->nSendSize < SendBufferSize())
                {
                    if (!pnode->vRecvGetData.empty() || (!pnode->vRecvMsg.empty() && pnode->vRecvMsg[0].complete()))
                    {
                        fSleep = false;
                    }
                }
            }
        }
        boost::this_thread::interruption_point();

        // Send messages
        {
            TRY_LOCK(pnode->cs_vSend, lockSend);
            if (lockSend)
                node_signals.SendMessages(pnode);
        }
        boost::this_thread::interruption_point();
    }

    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
            pnode->Release();
    }

    if (fSleep)
        MilliSleep(70);
}

void Network::ThreadMessageHandler()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);

    while (true)
    {
        MessageHandlerIteration();
    }
}


