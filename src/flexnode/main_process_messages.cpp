#include <src/net/net_services.h>
#include "../../test/flex_tests/flex_data/TestData.h"

#include "flexnode/main.h"

#include "flexnode/init.h"
#include "flexnode/MainFlexNode.h"
#include "net/net_cnode.h"

using namespace std;
using namespace boost;


#include "log.h"
#define LOG_CATEGORY "main_process_messages.cpp"

bool MainRoutine::AlreadyHave(const CInv& inv)
{
    return flexnode->HaveInventory(inv);
}

bool MainRoutine::IsInitialBlockDownload()
{
    return not flexnode->IsFinishedDownloading();
}

void MainRoutine::ProcessGetData(CNode* pfrom)
{
    std::deque<CInv>::iterator it = pfrom->vRecvGetData.begin();

    vector<CInv> vNotFound;

    LOCK(cs_main);

    while (it != pfrom->vRecvGetData.end())
    {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        const CInv &inv = *it;
        {
            boost::this_thread::interruption_point();
            it++;

            if (inv.type == MSG_BLOCK)
            {
                log_ << "block with hash: " << inv.hash << "\n";
//                MinedCreditMessage msg = creditdata[inv.hash]["msg"];
//                log_ << "that block has mined credit: "
//                << msg.mined_credit << "\n";
//                pfrom->PushMessage("block", msg);
//                log_ << "Sending minedcreditmessage "
//                << msg.GetHash() << "\n";
//                log_ << "hashes: " << msg.hash_list.short_hashes.size()
//                << "\n";
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(network->cs_mapRelay);
                    map<CInv, CDataStream>::iterator mi = network->mapRelay.find(inv);
                    if (mi != network->mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TX) {
//                    SignedTransaction tx = creditdata[inv.hash]["tx"];
//                    pfrom->PushMessage("tx", tx);
//                    std::cout << "Sending SignedTransaction "
//                    << tx.GetHash().ToString() << "\n";
//                    pushed = true;
                }
                if (!pushed) {
                    vNotFound.push_back(inv);
                }
            }

            // Track requests for our stuff.
            g_signals.Inventory(inv.hash);

            if (inv.type == MSG_BLOCK)
                break;
        }
    }

    pfrom->vRecvGetData.erase(pfrom->vRecvGetData.begin(), it);

    if (!vNotFound.empty()) {
        // Let the peer know that we didn't find what it asked for, so it doesn't
        // have to wait around forever.
        pfrom->PushMessage("notfound", vNotFound);
    }
}

bool MainRoutine::ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
    if (flexnode == NULL)
    {
        log_ << routine_name << " no flex_local_server available to process message: " << strCommand << "\n";
        return true;
    }
    LOCK(flexnode->mutex);

    RandAddSeedPerfmon();
    log_ << "received: " << strCommand << " (" << vRecv.size() << " bytes)\n";

    std::cout << "received: " << strCommand << " (" << vRecv.size() << " bytes)\n";

    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        log_ << "dropmessagestest DROPPING RECV MESSAGE\n";
        return true;
    }

    {
        LOCK(cs_main);
        StateOfNode(pfrom->GetId())->nLastBlockProcess = GetTimeMicros();
    }

    if (strCommand == "version")
    {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0)
        {
            pfrom->PushMessage("reject", strCommand, REJECT_DUPLICATE, string("Duplicate version message"));
            Misbehaving(pfrom->GetId(), 1);
            return false;
        }
        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;
        vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION)
        {
            // disconnect from peers older than this proto version
            log_ << "partner " << pfrom->addr
                 << " using obsolete version " << pfrom->nVersion
                 << "; disconnecting\n";
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
                               strprintf("Version must be %d or greater", MIN_PEER_PROTO_VERSION));
            std::cout << "obsolete peer\n";
            pfrom->fDisconnect = true;
            return false;
        }

        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (!vRecv.empty())
        {
            vRecv >> pfrom->strSubVer;
            pfrom->cleanSubVer = SanitizeString(pfrom->strSubVer);
        }
        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;
        if (!vRecv.empty())
            vRecv >> pfrom->fRelayTxes; // set to true after we get the first filter* message
        else
            pfrom->fRelayTxes = true;


        if (pfrom->fInbound && addrMe.IsRoutable())
        {
            pfrom->addrLocal = addrMe;
            network->SeenLocal(addrMe);
        }

        // Disconnect if we connected to ourself
        if (nNonce == network->nLocalHostNonce && nNonce > 1)
        {
            log_ << "connected to self at " << pfrom->addr << "; disconnecting\n";
            pfrom->fDisconnect = true;
            return true;
        }

        // Be shy and don't send version until we hear
        if (pfrom->fInbound)
        {
            pfrom->PushVersion();
        }

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);


        // Change version
        pfrom->PushMessage("verack");
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if (!pfrom->fInbound && !IsInitialBlockDownload())
        {
            // Advertise our address
            if (!fNoListen)
            {
                CAddress addr = network->GetLocalAddress(&pfrom->addr);
                if (addr.IsRoutable())
                    pfrom->PushAddress(addr);
            }

            // Get recent addresses
            if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || network->addrman.size() < 1000)
            {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
            network->addrman.Good(pfrom->addr);
        } else {
            if (((CNetAddr)pfrom->addr) == (CNetAddr)addrFrom)
            {
                network->addrman.Add(addrFrom, addrFrom);
                network->addrman.Good(addrFrom);
            }
        }

        pfrom->fSuccessfullyConnected = true;
        log_ << "receive version message: " << pfrom->cleanSubVer
             << " version " << pfrom->nVersion
             << " blocks=" << pfrom->nStartingHeight
             << " us=" << addrMe << " them=" << addrFrom
             << " peer=" << pfrom->addr << "\n";

        AddTimeData(pfrom->addr, nTime);
    }


    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        Misbehaving(pfrom->GetId(), 1);
        return false;
    }


    else if (strCommand == "verack")
    {
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
    }


    else if (strCommand == "addr")
    {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        // Don't want addr from older versions unless seeding
        if (pfrom->nVersion < CADDR_TIME_VERSION && network->addrman.size() > 1000)
            return true;
        if (vAddr.size() > 1000)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message addr size() = %u", vAddr.size());
        }

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        BOOST_FOREACH(CAddress& addr, vAddr)
        {
            boost::this_thread::interruption_point();

            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = (unsigned int) (nNow - 5 * 24 * 60 * 60);
            pfrom->AddAddressKnown(addr);
            bool fReachable = network->IsReachable(addr);
            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable())
            {
                // Relay to a limited number of other nodes
                {
                    LOCK(network->cs_vNodes);
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = hashSalt ^ (hashAddr<<32) ^ ((GetTime()+hashAddr)/(24*60*60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;

                    for (auto pnode : network->vNodes)
                    {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }
            // Do not store addresses outside our network
            if (fReachable)
                vAddrOk.push_back(addr);
        }
        network->addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
        {
            std::cout << "oneshot - disconnecting\n";
            pfrom->fDisconnect = true;
        }

    }


    else if (strCommand == "inv")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message inv size() = %u", vInv.size());
        }

        LOCK(cs_main);

        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            const CInv &inv = vInv[nInv];

            boost::this_thread::interruption_point();
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(inv);
            log_ << "net:  got inventory: "
                 << inv << " "
                 << (fAlreadyHave ? "have" : "new") << "\n";

            log_ << "hash is " << inv.hash << "\n";

            if (!fAlreadyHave && !IsInitialBlockDownload())
            {
                if (inv.type == MSG_BLOCK)
                    AddBlockToQueue(pfrom->GetId(), inv.hash);
                else
                    pfrom->AskFor(inv);
            }

            // Track requests for our stuff
            g_signals.Inventory(inv.hash);
        }
    }


    else if (strCommand == "getdata")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message getdata size() = %u", vInv.size());
        }

        if (fDebug || (vInv.size() != 1))
            LogPrint("net", "received getdata (%u invsz)\n", vInv.size());

        if ((fDebug && vInv.size() > 0) || (vInv.size() == 1))
            LogPrint("net", "received getdata for: %s\n", vInv[0].ToString());

        pfrom->vRecvGetData.insert(pfrom->vRecvGetData.end(), vInv.begin(), vInv.end());
        ProcessGetData(pfrom);
    }

    else if (strCommand == "getaddr")
    {
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = network->addrman.GetAddr();
        BOOST_FOREACH(const CAddress &addr, vAddr)
            pfrom->PushAddress(addr);
    }

    else if (strCommand == "ping")
    {
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce;
            // Echo the message back with the nonce. This allows for two useful features:
            //
            // 1) A remote node can quickly check if the connection is operational
            // 2) Remote nodes can measure the latency of the network thread. If this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // The nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            pfrom->PushMessage("pong", nonce);
        }
    }

    else if (strCommand == "pong")
    {
        int64_t pingUsecEnd = GetTimeMicros();
        uint64_t nonce = 0;
        size_t nAvail = (size_t) vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;

        if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;

            // Only process pong message if there is an outstanding ping (old ping without nonce should never pong)
            if (pfrom->nPingNonceSent != 0) {
                if (nonce == pfrom->nPingNonceSent) {
                    // Matching pong received, this ping is no longer outstanding
                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime > 0) {
                        // Successful ping time measurement, replace previous
                        pfrom->nPingUsecTime = pingUsecTime;
                    } else {
                        // This should never happen
                        sProblem = "Timing mishap";
                    }
                } else {
                    // Nonce mismatches are normal when pings are overlapping
                    sProblem = "Nonce mismatch";
                    if (nonce == 0) {
                        // This is most likely a bug in another implementation somewhere, cancel this ping
                        bPingFinished = true;
                        sProblem = "Nonce zero";
                    }
                }
            } else {
                sProblem = "Unsolicited pong without ping";
            }
        } else {
            // This is most likely a bug in another implementation somewhere, cancel this ping
            bPingFinished = true;
            sProblem = "Short payload";
        }

        if (!(sProblem.empty())) {
            LogPrint("net", "pong %s %s: %s, %x expected, %x received, %u bytes\n",
                pfrom->addr.ToString(),
                pfrom->cleanSubVer,
                sProblem,
                pfrom->nPingNonceSent,
                nonce,
                nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
    }

    else
    {
        flexnode->HandleMessage(strCommand, vRecv, pfrom);
    }



    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
            network->AddressCurrentlyConnected(pfrom->addr);


    return true;
}

// requires LOCK(cs_vRecvMsg)
bool MainRoutine::ProcessMessages(CNode* pfrom)
{
    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    if (!pfrom->vRecvGetData.empty())
        ProcessGetData(pfrom);

    // this maintains the order of responses
    if (!pfrom->vRecvGetData.empty()) return fOk;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end())
    {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        // get next message
        CNetMessage& msg = *it;

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, Params().MessageStart(), MESSAGE_START_SIZE) != 0) {
            log_ << "\n\nPROCESSMESSAGE: INVALID MESSAGESTART\n\n";
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid())
        {
            log_ << "\n\nPROCESSMESSAGE: ERRORS IN HEADER \n\n\n"
                 << hdr.GetCommand() << "\n";
            continue;
        }
        string strCommand = hdr.GetCommand();
        log_ << "ProcessMessages(): strCommand is " << strCommand << "\n";

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv = msg.vRecv;
        uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (nChecksum != hdr.nChecksum)
        {
            log_ << "ProcessMessages(" << strCommand
                 << ", " << nMessageSize << " bytes)"
                 <<" : CHECKSUM ERROR nChecksum="
                 << nChecksum << " hdr.nChecksum=" << hdr.nChecksum;
            continue;
        }

        // Process message
        bool fRet = false;
        try
        {
            fRet = ProcessMessage(pfrom, strCommand, vRecv);
            boost::this_thread::interruption_point();
        }
        catch (std::ios_base::failure& e)
        {
            pfrom->PushMessage("reject", strCommand, REJECT_MALFORMED, string("error parsing message"));
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from under-length message on vRecv
                log_ << "ProcessMessages(" << strCommand
                     << ", " << nMessageSize << " bytes) : Exception "
                     << "'" << e.what() << "' caught, normally caused by a message being shorter than its stated length\n";
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from over-long size
            }
            else
            {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        }
        catch (boost::thread_interrupted) {
            throw;
        }
        catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessages()");
        }

        if (!fRet)
            log_ << "ProcessMessage(" << strCommand << ", " << nMessageSize << " bytes) FAILED\n";

        break;
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}