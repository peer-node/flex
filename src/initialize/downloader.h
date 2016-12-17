// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef TELEPORT_INITIALDOWNLOADER
#define TELEPORT_INITIALDOWNLOADER

#include "database/memdb.h"
#include "teleportnode/wallet.h"
#include "net/net.h"
#include "credits/creditutil.h"
#include "teleportnode/calendar.h"
#include "relays/relays.h"
#include <deque>
#include <openssl/rand.h>
#include "relays/relaystate.h"
#include "teleportnode/main.h"
#include "teleportnode/orphanage.h"
#include "initialize/datamessages.h"

#include "log.h"
#define LOG_CATEGORY "downloader.h"


inline uint32_t RandomUint32(uint32_t maximum)
{
    uint32_t n;
    RAND_bytes(UBEGIN(n), 4);
    return n % maximum;
}


class DataHandler
{
public:
    DataHandler()
    { }

    uint160 SendDataRequest(std::vector<uint160> list_expansion_failed_msgs,
                            std::vector<uint160> unknown_hashes,
                            std::vector<uint160> credit_hashes,
                            std::vector<Point> deposit_addresses,
                            CNode *peer)
    {
        DataRequestMessage request;
        request.list_expansion_failed_msgs = list_expansion_failed_msgs;
        request.unknown_hashes = unknown_hashes;
        request.credit_hashes = credit_hashes;
        request.deposit_addresses = deposit_addresses;
        
        return SendDataRequest(request, peer);
    }

    uint160 SendDataRequest(DataRequestMessage request, CNode *peer)
    {
        uint160 request_hash = request.GetHash160();
        initdata[request_hash]["requested_data_from"] = peer->GetId();
        log_ << "Sending data request: " << request;

        peer->PushMessage("reqinitdata", "getdata", request);
        return request_hash;
    }

    void RequestBatch(uint160 credit_hash, CNode *peer)
    {
        std::vector<uint160> hashes;
        hashes.push_back(credit_hash);
        SendDataRequest(hashes, hashes, 
                        std::vector<uint160>(), 
                        std::vector<Point>(),
                        peer);
    }

    void HandleOrphans(uint160 hash);

    void HandleRequestedDataMessage(RequestedDataMessage& msg, CNode *peer);

    void HandleInitialDataMessage(InitialDataMessage& msg, CNode *peer);

    bool CheckInitialDataIsPresent(InitialDataMessage& msg);

    bool ValidateInitialDataMessage(InitialDataMessage& msg);
};


class CalendarHandler
{
public:
    CalendarHandler() { }

    void HandleBadWorkFoundInCalendar(Calendar& calendar,
                                      CalendarFailureDetails& details,
                                      CNode *peer)
    {
        calendardata[details.diurn_root]["failure_details"] = details;
        calendardata[details.credit_hash]["failure_details"] = details;
        RemoveEveryThingAfter(details.credit_hash);
        peer->PushMessage("initdata", "badcalendar", details);
        LOCK(cs_main);
        Misbehaving(peer->GetId(), 50);
        return;
    }

    void HandleBadCalendarReport(CalendarFailureDetails details, CNode *peer)
    {
        TwistWorkCheck check = details.check;
        if (!check.VerifyInvalid())
        {
            log_ << "calendar failure details invalid!\n";
            LOCK(cs_main);
            Misbehaving(peer->GetId(), 100);
            return;
        }
        calendardata[details.diurn_root]["failure_details"] = details;
        calendardata[details.credit_hash]["failure_details"] = details;
        RemoveEveryThingAfter(details.credit_hash);
        return;
    }

    void RemoveEveryThingAfter(uint160 bad_credit_hash);

    void HandleCalendarMessage(CalendarMessage calendar_msg, CNode *peer);

    void HandleInvalidCalendar(uint160 credit_hash, CNode *peer)
    {
        uint160 work = initdata[credit_hash].Location("reported_work");
        initdata.RemoveFromLocation("reported_work", work);
        Misbehaving(peer->GetId(), 100);
    }


    void HandleApparentlyMostWorkCalendar(Calendar calendar,
                                          uint160 work,
                                          CNode *peer);

    bool Scrutinize(Calendar calendar, 
                    uint64_t microseconds, 
                    CalendarFailureDetails& details);

    void HandleNewBest(Calendar calendar, CNode *peer);

    void RequestInitialData(uint160 credit_hash, CNode *peer);
};


void HandleNewBestCalendar(Calendar calendar);

class Downloader
{
public:
    bool started_downloading;
    bool finished_downloading;
    CalendarHandler calendarhandler;
    DataHandler datahandler;

    Downloader();

    void Initialize();

    void HandleMessage(CDataStream ss, CNode *peer);

    void RequestTips()
    {
        foreach_(CNode* peer, vNodes)
            peer->PushMessage("reqinitdata", "gettip");
        started_downloading = true;
        finished_downloading = false;
    }

    void RememberPeerHasTip(uint160 credit_hash, CNode *peer)
    {
        int32_t node_id = peer->GetId();
        std::vector<int32_t> node_ids = initdata[credit_hash]["nodes"];

        if (!VectorContainsEntry(node_ids, node_id))
            node_ids.push_back(node_id);

        initdata[credit_hash]["nodes"] = node_ids;
    }

    void HandleTip(MinedCreditMessage msg, CNode *peer);

    void HandleDiurnBranches(DiurnBranchMessage msg, CNode *peer);

    void RequestCalendar(uint160 credit_hash, CNode *peer)
    {
        peer->PushMessage("reqinitdata", "getcalendar", credit_hash);
        initdata[credit_hash]["requested_calendar_from"] = peer->GetId();
        
        initdata[credit_hash].Location("calendar_reqtime") = GetTimeMicros();
    }

    void RequestCreditsMatchingWallet()
    {
        MatchingCreditsRequestMessage msg(0);
        msg.DoWork();
        log_ << "RequestCreditsMatchingWallet(): work is ok: "
             << msg.CheckWork() << "\n";
        uint160 request_hash = msg.GetHash160();

        foreach_(CNode* peer, vNodes)
        {
            peer->PushMessage("reqinitdata", "getmatchingcredits", msg);
            uint32_t peer_id = peer->GetId();
            initdata[request_hash][peer_id] = true;
        }
    }

    void HandleMatchingCreditsMessage(MatchingCreditsMessage msg, CNode *peer);

    void RequestMissingData(MinedCreditMessage msg, CNode *peer)
    {
        uint160 credit_hash = msg.mined_credit.GetHash160();
        DataRequestMessage request;

        log_ << "requesting missing data\n";
        std::vector<uint32_t> hashes;
        uint160 prev_hash = msg.mined_credit.previous_mined_credit_hash;

        if (prev_hash > 0 && !creditdata[prev_hash].HasProperty("msg"))
            request.credit_hashes.push_back(prev_hash);

        if (!msg.hash_list.RecoverFullHashes())
            request.list_expansion_failed_msgs.push_back(credit_hash);

        try
        {
            RelayState state = GetRelayState(prev_hash);
            request.unknown_hashes = MissingDataNeededToCalculateFees(state);
        }
        catch (RecoverHashesFailedException& e)
        {
            request.list_expansion_failed_msgs.push_back(e.credit_hash);
        }
        catch (NoKnownRelayStateException& e)
        {
            uint32_t peer_id = peer->GetId();
            if (initdata[peer_id]["requested_calendar"])
                return;
            RequestCalendar(credit_hash, peer);
            initdata[peer_id]["requested_calendar"] = true;
            return;
        }

        datahandler.SendDataRequest(request, peer);

        uint160 request_hash = request.GetHash160();

        log_ << "queueing credit " << credit_hash 
             << " behind data request " << request << "\n";

        initdata[request_hash]["queued_batch"] = credit_hash;
    }
};

#endif