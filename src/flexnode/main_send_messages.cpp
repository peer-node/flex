#include <src/net/net_services.h>
#include "../../test/flex_tests/flex_data/TestData.h"

#include "flexnode/main.h"
#include "net/net_cnode.h"

#include "log.h"
#define LOG_CATEGORY "main_send_messages.cpp"

using namespace std;

bool MainRoutine::AlreadyHave(const CInv& inv)
{
    return (bool) invs_received.count(inv);
}

bool MainRoutine::SendMessages(CNode* pto)
{
    {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;

        //
        // Message: ping
        //
        bool pingSend = false;
        if (pto->fPingQueued)
        {
            // RPC ping request by user
            pingSend = true;
        }
        if (pto->nLastSend && GetTime() - pto->nLastSend > 30 * 60 && pto->vSendMsg.empty()) {
            // Ping automatically sent as a keepalive
            pingSend = true;
        }
        if (pingSend)
        {
            uint64_t nonce = 0;
            while (nonce == 0)
                RAND_bytes((unsigned char*)&nonce, sizeof(nonce));

            pto->nPingNonceSent = nonce;
            pto->fPingQueued = false;

            // Take timestamp as close as possible before transmitting ping
            pto->nPingUsecStart = GetTimeMicros();
            pto->PushMessage("ping", nonce);
        }

        TRY_LOCK(cs_main, lockMain); // Acquire cs_main for IsInitialBlockDownload() and CNodeState()
        if (!lockMain)
            return true;

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (GetTime() - nLastRebroadcast > 24 * 60 * 60)
        {
            {
                LOCK(network->cs_vNodes);
                BOOST_FOREACH(CNode* pnode, network->vNodes)
                {
                    // Periodically clear setAddrKnown to allow refresh broadcasts
                    if (nLastRebroadcast)
                        pnode->setAddrKnown.clear();

                    // Rebroadcast our address
                    if (!fNoListen)
                    {
                        CAddress addr = network->GetLocalAddress(&pnode->addr);
                        if (addr.IsRoutable())
                            pnode->PushAddress(addr);
                    }
                }
            }
            nLastRebroadcast = GetTime();
        }

        if (pto->recent_misbehavior != 0)
        {
            Misbehaving(pto->GetId(), pto->recent_misbehavior);
            pto->recent_misbehavior = 0;
        }

        CNodeState &state = *StateOfNode(pto->GetId());
        if (state.fShouldBan)
        {
            if (pto->addr.IsLocal())
                log_ << "Warning: not banning local node " << pto->addr << "\n";
            else
            {
                log_ << "main_send_messages.cpp: banning\n";
                pto->fDisconnect = true;
                CNode::Ban(pto->addr);
            }
            state.fShouldBan = false;
        }

        // Start block sync
        if (pto->fStartSync) {
            pto->fStartSync = false;
        }

        g_signals.Broadcast();

        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            BOOST_FOREACH(const CInv& inv, pto->vInventoryToSend)
            {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second)
                {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000)
                    {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);


        // Detect stalled peers. Require that blocks are in flight, we haven't
        // received a (requested) block in one minute, and that all blocks are
        // in flight for over two minutes, since we first had a chance to
        // process an incoming block.
        int64_t nNow = GetTimeMicros();
        if (!pto->fDisconnect && state.nBlocksInFlight &&
            state.nLastBlockReceive < state.nLastBlockProcess - BLOCK_DOWNLOAD_TIMEOUT*1000000 &&
            state.vBlocksInFlight.front().nTime < state.nLastBlockProcess - 2*BLOCK_DOWNLOAD_TIMEOUT*1000000)
        {
            log_ << "Peer " << state.name
                 << " is stalling block download, disconnecting\n";
            log_ << "receive < process - ... : "
                 << (state.nLastBlockReceive
                     < state.nLastBlockProcess
                            - BLOCK_DOWNLOAD_TIMEOUT * 1000000)
                 << "\n";
            log_ << "in flight: " << state.nBlocksInFlight;
            log_ << "receive: " << state.nLastBlockReceive;
            log_ << "process: " << state.nLastBlockProcess;
            pto->fDisconnect = true;
        }
        //
        // Message: getdata (blocks)
        //
        vector<CInv> vGetData;
        while (!pto->fDisconnect && state.nBlocksToDownload
                && state.nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER)
        {
            uint256 hash = state.vBlocksToDownload.front();
            vGetData.push_back(CInv(MSG_BLOCK, hash));
            MarkBlockAsInFlight(pto->GetId(), hash);
            log_ << "net: Requesting block " << hash << " from " << state.name << "\n";

            if (vGetData.size() >= 1000)
            {
                pto->PushMessage("getdata", vGetData);
                vGetData.clear();
            }
        }

        //
        // Message: getdata (non-blocks)
        //
        while (!pto->fDisconnect && !pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(inv))
            {
                if (fDebug)
                    log_ << "sending getdata: " << inv << "\n";
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000)
                {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);

    }
    return true;
}
