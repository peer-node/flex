// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "flexnode/flexnode.h"

using namespace std;

#include "log.h"
#define LOG_CATEGORY "downloader.cpp"


#define SCRUTINY_TIME 3000000 // microseconds



void StartUsing(uint160 latest_credit_hash)
{
    flexnode.calendar = calendardata[latest_credit_hash]["calendar"];

    log_ << "StartUsing(): " << latest_credit_hash << "\n";
    log_ << "calendar is:" << flexnode.calendar;

    foreach_(Calend calend, flexnode.calendar.calends)
    {
        uint160 calend_hash = calend.mined_credit.GetHash160();
        calendardata[calend_hash]["is_calend"] = true;
    }

    uint160 calend_hash = GetCalendCreditHash(latest_credit_hash);

    flexnode.InitiateChainAtCalend(calend_hash);

    foreach_(uint160 credit_hash, 
             GetCreditHashesSinceCalend(calend_hash, latest_credit_hash))
    {
        MinedCreditMessage msg = creditdata[credit_hash]["msg"];
        flexnode.AddBatchToTip(msg);
    }

    flexnode.downloader.finished_downloading = true;
    log_ << "StartUsing() finished. Initialization complete\n"
         << "requesting credits paid to me from peers\n";
    flexnode.downloader.RequestCreditsMatchingWallet();
}



/*****************
 *  Downloader
 */

    Downloader::Downloader():
        started_downloading(false),
        finished_downloading(false)
    { }

    void Downloader::Initialize()
    {
        return;
    }

    void Downloader::HandleMessage(CDataStream ss, CNode *peer)
    {
        std::string type;
        ss >> type;

        log_ << "Downloader::HandleMessage(): " << type << "\n";

        if (type == "tip")
        {
            MinedCreditMessage msg;
            ss >> msg;
            HandleTip(msg, peer);
        }
        else if (type == "calendar")
        {
            CalendarMessage msg;
            ss >> msg;
            calendarhandler.HandleCalendarMessage(msg, peer);
        }
        else if (type == "badcalendar")
        {
            CalendarFailureDetails details;
            ss >> details;
            calendarhandler.HandleBadCalendarReport(details, peer);
        }
        else if (type == "requested_data")
        {
            RequestedDataMessage msg;
            ss >> msg;
            datahandler.HandleRequestedDataMessage(msg, peer);
        }
        else if (type == "initial_data")
        {
            InitialDataMessage msg;
            ss >> msg;
            datahandler.HandleInitialDataMessage(msg, peer);
        }
        else if (type == "diurn_branches")
        {
            DiurnBranchMessage msg;
            ss >> msg;
            HandleDiurnBranches(msg, peer);
        }
        else if (type == "matching_credits")
        {
            MatchingCreditsMessage msg;
            ss >> msg;
            HandleMatchingCreditsMessage(msg, peer);
        }
    }

    void Downloader::HandleMatchingCreditsMessage(MatchingCreditsMessage msg,
                                                  CNode *peer)
    {
        log_ << "handling:" << msg;
        uint32_t peer_id = peer->GetId();
        if (!initdata[msg.request_hash][peer_id])
        {
            log_ << "request with hash " << msg.request_hash
                 << " was never send to peer " << peer->GetId() << "\n";
            Misbehaving(peer->GetId(), 100);
            return;
        }
        flexnode.wallet.HandleReceivedCredits(msg.credits);
    }

    void Downloader::HandleDiurnBranches(DiurnBranchMessage msg, CNode *peer)
    {
        log_ << "handling:" << msg;
        std::vector<uint160> branches_received;
        if (msg.credit_hashes.size() != msg.branches.size())
        {
            Misbehaving(peer->GetId(), 100);
            return;
        }
        foreach_(const uint160& credit_hash, msg.credit_hashes)
        {
            if (!initdata[credit_hash]["requested"])
            {
                Misbehaving(peer->GetId(), 100);
                return;
            }
        }
        for (uint32_t i = 0; i < msg.credit_hashes.size(); i++)
        {
            uint160 diurn_root = msg.branches[i].back();
            uint32_t branch_start_position = 1;
            if (EvaluateBranchWithHash(msg.branches[i], msg.credit_hashes[i],
                                       branch_start_position) != diurn_root)                
            {
                log_ << "bad diurn branch for " << msg.credit_hashes[i]
                     << " in diurn branch msg\n",
                Misbehaving(peer->GetId(), 100);
                return;
            }
            if (flexnode.calendar.ContainsDiurn(diurn_root))
            {
                creditdata[msg.credit_hashes[i]]["branch"] = msg.branches[i];
                initdata[msg.credit_hashes[i]]["requested"] = false;
                branches_received.push_back(msg.credit_hashes[i]);
            }
        }
        flexnode.chainer.HandleDiurnBranchesReceived(branches_received, peer);
    }
     
    void Downloader::HandleTip(MinedCreditMessage msg, CNode *peer)
    {
        uint160 msg_hash = msg.GetHash160();

        log_ << "Downloader(): received tip message "
             << msg_hash << "\n";
     
        uint160 credit_hash = msg.mined_credit.GetHash160();
        RememberPeerHasTip(credit_hash, peer);

        if (initdata[msg_hash]["received"])
            return;

        if (msg.proof.memoryseed != msg.mined_credit.GetHash())
        {
            log_ << "bad memory seed: "
                 << msg.proof.memoryseed << " vs "
                 << msg.mined_credit.GetHash() << "\n";
            Misbehaving(peer->GetId(), 100);
            return;
        }

        if (!msg.proof.WorkDone())
        {
            log_ << "proof failed quick check!\n";
            Misbehaving(peer->GetId(), 100);
            return;
        }
        
        initdata[credit_hash].Location("reported_work")
            = msg.mined_credit.total_work;

        if (credit_hash == GetLastHash(initdata, "reported_work"))
        {
            log_ << "most work reported so far: "
                 << msg.mined_credit.total_work << "\n";
            RequestCalendar(credit_hash, peer);

            creditdata[credit_hash]["mined_credit"] = msg.mined_credit;
            creditdata[credit_hash]["msg"] = msg;
        }
        else
        {
            uint160 last_hash = GetLastHash(initdata, "reported_work");
            uint160 max_work = initdata[last_hash].Location("reported_work");
            log_ << last_hash
                 << " has more work, namely "
                 << max_work << "\n";
        }
    }

/*
 *  Downloader
 ****************/


/******************
 *  DataHandler
 */

    void DataHandler::HandleOrphans(uint160 hash)
    {
        msgdata[hash]["handled"] = true;
        log_ << "DataHandler::HandleOrphans: " << hash << "\n";
        vector<MinedCreditMessage> non_orphans
            = flexnode.chainer.orphanage.NuggetNonOrphansAfterHash(hash);
        foreach_(MinedCreditMessage msg, non_orphans)
        {
            log_ << "handling erstwhile orphan msg with credit "
                 << msg.mined_credit << "\n";
            flexnode.chainer.HandleMinedCreditMessage(msg, NULL);
        }
        vector<uint160> orphans = msgdata[hash]["orphans"];
        flexnode.relayhandler.HandleOrphans(hash);
        msgdata[hash]["orphans"] = orphans;
        flexnode.tradehandler.HandleOrphans(hash);
    }

    bool DataHandler::CheckInitialDataIsPresent(InitialDataMessage& msg)
    {
        foreach_(MinedCreditMessage credit_msg, msg.mined_credit_messages)
        {
            uint160 credit_hash = credit_msg.mined_credit.GetHash160();
            uint160 prev_hash;
            prev_hash = credit_msg.mined_credit.previous_mined_credit_hash;

            if (prev_hash != 0 && (!calendardata[credit_hash]["is_calend"])
                && !creditdata[prev_hash].HasProperty("msg"))
            {
                log_ << "CheckInitialDataIsPresent(): missing " 
                     << prev_hash << " preceding the mined credit"
                     << credit_msg.mined_credit;
                return false;
            }
            if (!credit_msg.hash_list.RecoverFullHashes())
            {
                log_ << "couldn't recover hashes for "
                     << credit_msg
                     << credit_msg.mined_credit;
                foreach_(const uint32_t short_hash, credit_msg.hash_list.short_hashes)
                {
                    log_ << short_hash << "\n";
                    std::vector<uint160> matches = hashdata[short_hash]["matches"];
                    log_ << "matches: " << matches.size() << "\n";
                    foreach_(const uint160 match, matches)
                        log_ << match << "\n";
                }
                return false;
            }
        }
        MinedCredit calend = creditdata[msg.calend_hash]["mined_credit"];

        if (calendardata[calend.previous_mined_credit_hash]["is_calend"])
        {
            uint160 previous_calend_hash = calend.previous_mined_credit_hash;
            MinedCredit previous_calend;
            previous_calend = creditdata[previous_calend_hash]["mined_credit"];

            if (msg.preceding_relay_state.GetHash160()
                != previous_calend.relay_state_hash)
            {
                log_ << "CheckInitialDataIsPresent(): missing preceding "
                     << "relay state " << previous_calend.relay_state_hash
                     << " needed to calculate fees included in calend\n"
                     << "wrong state is included: "
                     << msg.preceding_relay_state.GetHash160() << "\n";

                log_ << "this calend is " << calend
                     << "previous calend was " << previous_calend << "\n";
                return false;
            }
        }
        RelayState state = msg.calend_relay_state;
        foreach_(HashToPointMap::value_type item, state.join_hash_to_relay)
        {
            uint160 join_hash = item.first;
            Point relay = item.second;
            if (!msgdata[join_hash].HasProperty("join"))
            {
                log_ << "missing join: " << join_hash
                     << " from " << relay << "\n";
                return false;
            }
            RelayJoinMessage join = msgdata[join_hash]["join"];
            if (!creditdata[join.credit_hash].HasProperty("mined_credit"))
            {
                log_ << "missing credit " << join.credit_hash
                     << " from" << join << "\n";
                return false;
            }
        }
        foreach_(PointToMsgMap::value_type item,
                 state.disqualifications)
        {
            std::set<uint160> message_hashes = item.second;
            
            foreach_(const uint160 message_hash, message_hashes)
            {
                string type = msgdata[message_hash]["type"];
                if (type == "" or !msgdata[message_hash].HasProperty(type))
                {
                    log_ << "missing disqualification "
                         << message_hash << "\n";
                    return false;
                }
            }
        }
        foreach_(PointToMsgMap::value_type item,
                 state.subthreshold_succession_msgs)
        {
            std::set<uint160> message_hashes = item.second;
            
            foreach_(const uint160 message_hash, message_hashes)
            {
                string type = msgdata[message_hash]["type"];
                if (type == "" or !msgdata[message_hash].HasProperty(type))
                {
                    log_ << "missing succession message "
                         << message_hash << "\n";
                    return false;
                }
            }
        }
        return true;
    }

    bool DataHandler::ValidateInitialDataMessage(InitialDataMessage& msg)
    {
        if (!CheckInitialDataIsPresent(msg))
        {
            log_ << "ValidateInitialDataMessage(): missing data\n";
            return false;
        }
        std::vector<uint160> credit_hashes;
        if (msg.calend_hash != GetCalendCreditHash(msg.credit_hash))
        {
            log_ << "ValidateInitialDataMessage(): wrong calend hash\n"
                 << msg.calend_hash << " vs "
                 << GetCalendCreditHash(msg.credit_hash) << "\n";
            return false;
        }

        log_ << "ValidateInitialDataMessage(): about to call "
             << "GetCreditHashesSinceCalend with " << msg.calend_hash
             << " and " << msg.credit_hash << "\n";

        credit_hashes = GetCreditHashesSinceCalend(msg.calend_hash,
                                                   msg.credit_hash);

        uint160 previous_hash = msg.calend_hash;
        foreach_(uint160 credit_hash, credit_hashes)
        {
            MinedCreditMessage msg = creditdata[credit_hash]["msg"];
            if (msg.mined_credit.previous_mined_credit_hash != previous_hash)
            {
                log_ << credit_hash << " comes after "
                     << msg.mined_credit.previous_mined_credit_hash
                     << ", not " << previous_hash << "!\n";
                return false;
            }
            if (!msg.Validate())
            {
                log_ << "can't validate " << credit_hash << "\n"
                     << "ValidateInitialDataMessage failed\n";
                return false;
            }
            previous_hash = credit_hash;
        }
        return true;
    }

    void DataHandler::HandleInitialDataMessage(InitialDataMessage& msg,
                                               CNode *peer)
    {
        log_ << "HandleInitialDataMessage(): " << msg;
        // int request_peer = initdata[msg.request_hash]["requested_data_from"];
        // if (!peer->GetId() == request_peer)
        // {
        //     log_ << "Received unsolicited initial data message from "
        //          << peer->GetId() << "\n";
        //     LOCK(cs_main);
        //     Misbehaving(peer->GetId(), 100);
        //     return;
        // }

        msg.StoreData();
        
        if (!ValidateInitialDataMessage(msg))
        {
            log_ << "Failed to validate InitialDataMessage\n";
            return;
        }
        log_ << "Initial Data Message validated\n";
        

        uint160 work = initdata[msg.credit_hash].Location("reported_work");
        initdata[msg.credit_hash].Location("verified_work") = work;
        if (GetLastHash(initdata, "verified_work") == msg.credit_hash)
        {
            log_ << msg.credit_hash << " has the most verified work\n";
            foreach_(MinedCredit mined_credit, msg.mined_credits)
            {
                uint160 credit_hash = mined_credit.GetHash160();
                msgdata[credit_hash]["received"] = true;
                AddToMainChain(credit_hash);
                log_ << credit_hash << " has been set in the main chain: "
                     << InMainChain(credit_hash) << "\n";
            }
            foreach_(MinedCreditMessage msg_, msg.mined_credit_messages)
            {
                uint160 credit_hash = msg_.mined_credit.GetHash160();
                msgdata[credit_hash]["received"] = true;
                AddToMainChain(credit_hash);
                log_ << credit_hash << " has been set in the main chain: "
                     << InMainChain(credit_hash) << "\n";
            }
            StartUsing(msg.credit_hash);
        }
    }

    void DataHandler::HandleRequestedDataMessage(RequestedDataMessage& msg,
                                                 CNode *peer)
    {
        log_ << "HandleRequestedDataMessage(): " << msg;

        int32_t request_peer
            = initdata[msg.data_request_hash]["requested_data_from"];
        
        if (request_peer != peer->GetId())
        {
            Misbehaving(peer->GetId(), 100);
            log_ << "Received response from wrong peer! "
                 << peer->GetId() << " vs " << request_peer << "\n";
            return;
        }

        msg.StoreData();
        
        foreach_(const SignedTransaction& tx, msg.txs)
        {
            uint160 txhash = tx.GetHash160();
            log_ << "handling transaction " << txhash << "\n";
            
            if (flexnode.downloader.finished_downloading)
                flexnode.chainer.HandleTransaction(tx, peer);
            HandleOrphans(txhash);
        }
        map<uint160,uint160> relay_states_to_credit_hashes;

        foreach_(const MinedCreditMessage msg_, msg.mined_credit_messages)
        {
            uint160 credit_hash = msg_.mined_credit.GetHash160();
            relay_states_to_credit_hashes[msg_.mined_credit.relay_state_hash]
                = credit_hash;
            log_ << "relay state " << msg_.mined_credit.relay_state_hash
                 << " has credit hash " << credit_hash << "\n";
            log_ << "handling mined credit msg " << credit_hash << "\n";
            if (flexnode.downloader.finished_downloading)
                flexnode.chainer.HandleMinedCreditMessage(msg_, peer);
            HandleOrphans(credit_hash);
        }
        for(uint32_t i = 0; i < msg.relay_message_contents.size(); i++)
        {
            string_t type(msg.relay_message_types[i].begin(),
                          msg.relay_message_types[i].end());
            CDataStream ss(msg.relay_message_contents[i],
                           SER_NETWORK, CLIENT_VERSION);

            uint160 hash = Hash160(ss.begin(), ss.end());
            log_ << "handling relay message " << hash 
                 << " of type " << type << "\n";

            flexnode.relayhandler.HandleMessage(hash);

            HandleOrphans(hash);
        }

        foreach_(DepositAddressHistory history, msg.address_histories)
        {
            if (history.Validate())
                history.HandleData();
        }

        uint160 queued_batch = initdata[msg.data_request_hash]["queued_batch"];
        if (queued_batch != 0)
        {
            MinedCreditMessage msg_ = creditdata[queued_batch]["msg"];
            log_ << "handling queued credit " << queued_batch;
            flexnode.chainer.HandleMinedCreditMessage(msg_, peer);
            initdata.EraseProperty(msg.data_request_hash, "queued_batch");
        }

        uint160 queued_ack_hash
            = initdata[msg.data_request_hash]["queued_ack"];
        if (queued_ack_hash != 0)
        {
            TransferAcknowledgement ack
                = msgdata[queued_ack_hash]["transfer_ack"];
            initdata.EraseProperty(msg.data_request_hash, "queued_ack");
            flexnode.deposit_handler.HandleTransferAcknowledgement(ack);
        }
    }

/*
 *  DataHandler
 ******************/



uint32_t FindFork(Calendar calendar1, Calendar calendar2)
{
    for (uint32_t i = 0; i < calendar1.Size(); i++)
    {
        if (i >= calendar2.Size() ||
            calendar1.calends[i] != calendar2.calends[i])
        {
            return i;
        }
    }
    return calendar1.Size();
}

/*********************
 *  CalendarHandler
 */

    void CalendarHandler::RemoveEveryThingAfter(uint160 bad_credit_hash)
    {
        std::vector<uint160> children = creditdata[bad_credit_hash]["children"];
        foreach_(const uint160& child_hash, children)
            RemoveEveryThingAfter(child_hash);
        uint160 zero = 0;
        calendardata[bad_credit_hash].Location("work") = zero;
        calendardata[bad_credit_hash].Location("reported_work") = zero;

        if (InMainChain(bad_credit_hash))
        {
            RemoveBlocksAndChildren(bad_credit_hash);
            if (bad_credit_hash == flexnode.previous_mined_credit_hash)
                flexnode.chainer.RemoveBatchFromTip();
        }
    }
    
    void CalendarHandler::HandleCalendarMessage(CalendarMessage calendar_msg, 
                                                CNode *peer)
    {
        Calendar calendar = calendar_msg.calendar;
        log_ << "HandleCalendarMessage: got calendar: " << calendar;
        uint160 credit_hash = calendar.LastMinedCreditHash();
        int32_t request_peer = initdata[credit_hash]["requested_calendar_from"];
        if (request_peer != peer->GetId())
        {
            log_ << "Unsolicited calendar! " << credit_hash << "\n";
            Misbehaving(peer->GetId(), 100);
            return;
        }

        if (!calendar.Validate())
        {
            log_ << "calendar failed validation\n";
            HandleInvalidCalendar(credit_hash, peer);
            return;
        }
        
        CalendarFailureDetails details;

        if (!calendar.SpotCheckWork(details))
        {
            log_ <<"work spot check failed\n";
            HandleBadWorkFoundInCalendar(calendar, details, peer);
            return;
        }
        log_ << "storing calendar with credit hash: " << credit_hash << "\n";
        calendardata[credit_hash]["calendar"] = calendar;

        MinedCredit credit = creditdata[credit_hash]["mined_credit"];

        uint160 reported_work = credit.total_work + credit.difficulty;
        uint160 calendar_work = calendar.TotalWork();

        if (calendar_work < reported_work)
        {
            log_ << "not enough work in calendar " << calendar_work << " vs "
                 << reported_work << "\n"
                 << "calendar is " << calendar << " and credit is" << credit;
            HandleInvalidCalendar(credit_hash, peer);
            return;
        }

        if (credit_hash == GetLastHash(initdata, "reported_work"))
        {
            log_ << "apparently this calendar has the most work\n";
            HandleApparentlyMostWorkCalendar(calendar, reported_work, peer);
        }
    }

    void CalendarHandler::HandleApparentlyMostWorkCalendar(Calendar calendar,
                                                           uint160 work,
                                                           CNode *peer)
    {
        log_ << "HandleApparentlyMostWorkCalendar()\n";
        CalendarFailureDetails details;
        if (calendar.Size() == 0 ||
            Scrutinize(calendar, SCRUTINY_TIME, details))
        {
            log_ << "Scrutiny successful\n";
            uint160 credit_hash = calendar.LastMinedCreditHash();
            log_ << "credit hash is " << credit_hash << "\n";
            creditdata[credit_hash].Location("work") = work;
            log_ << "Handling new best\n";
            HandleNewBest(calendar, peer);
        }
        else
        {
            HandleBadWorkFoundInCalendar(calendar, details, peer);
        }
    }

    bool CalendarHandler::Scrutinize(Calendar calendar, 
                                     uint64_t microseconds, 
                                     CalendarFailureDetails& details)
    {
        log_ << "Scrutinizing calendar: " << calendar;
        uint32_t fork_position = 0;
        
        fork_position = FindFork(calendar, flexnode.calendar);
        
        uint64_t now = GetTimeMicros();
        while (GetTimeMicros() - now < microseconds)
        {
            uint32_t calend_position;
            calend_position = RandomUint32(calendar.Size() - fork_position);
            calend_position += fork_position;
            TwistWorkCheck check 
                = calendar.calends[calend_position].proof.SpotCheck();
            if (!check.Valid())
            {
                details.diurn_root = calendar.calends[calend_position].Root();
                details.credit_hash
                 = calendar.calends[calend_position].mined_credit.GetHash160();
                details.check = check;
                return false;
            }
        }
        return true;
    }

    void CalendarHandler::HandleNewBest(Calendar calendar, CNode *peer)
    {
        uint160 credit_hash = calendar.LastMinedCreditHash();
        calendardata[credit_hash]["calendar"] = calendar;

        log_ << "HandleNewBest(): calendar is " << calendar;
        if (calendar.calends.size() > 0)
        {
            foreach_(Calend calend, calendar.calends)
            {
                uint160 credit_hash = calend.mined_credit.GetHash160();
                MinedCreditMessage calend_msg = (MinedCreditMessage)calend;
                RecordBatch(calend_msg);
                StoreMinedCreditMessage(calend_msg);
                calendardata[credit_hash]["is_calend"] = true;
                calendardata[calend.Root()]["credit_hash"] = credit_hash;
                log_ << credit_hash << " is recognized as a calend\n";
            }
        }
        foreach_(MinedCreditMessage& msg, 
                 calendar.current_diurn.credits_in_diurn)
        {
            RecordBatch(msg);
            StoreMinedCreditMessage(msg);
        }
        RequestInitialData(credit_hash, peer);
    }

    void CalendarHandler::RequestInitialData(uint160 credit_hash, CNode *peer)
    {
        peer->PushMessage("reqinitdata", "getinitialdata", credit_hash);
    }

/*
 *  CalendarHandler
 *********************/
