#ifndef TELEPORT_DATASERVER
#define TELEPORT_DATASERVER

#include "net/net.h"
#include "teleportnode/main.h"
#include "credits/creditutil.h"
#include "teleportnode/calendar.h"
#include "relays/relays.h"
#include "relays/relaystate.h"
#include "initialize/datamessages.h"


#include "log.h"
#define LOG_CATEGORY "dataserver.h"


class DataServer
{
public:
    void HandleMessage(CDataStream ss, CNode *peer)
    {
        std::string type;
        ss >> type;

        log_ << "DataServer::HandleMessage(): " << type << "\n";

        if (type == "gettip")
        {
            SendTip(peer);
        }
        else if (type == "getcalendar")
        {   uint160 credit_hash;
            ss >> credit_hash;
            log_ << "Calendar requested for " << credit_hash << "\n";
            SendCalendar(credit_hash, peer);
        }
        else if (type == "getdata")
        {
            DataRequestMessage request;
            ss >> request;
            SendRequestedDataMessage(request, peer);
        }
        else if (type == "getinitialdata")
        {
            uint160 credit_hash;
            ss >> credit_hash;
            log_ << "DataServer: sending initial_data for " << credit_hash
                 << "\n";
            SendInitialDataMessage(credit_hash, peer);
        }
        else if (type == "get_diurn_branches")
        {
            std::vector<uint160> credit_hashes;
            ss >> credit_hashes;
            SendDiurnBranches(credit_hashes, peer);
        }
        else if (type == "getmatchingcredits")
        {
            MatchingCreditsRequestMessage request;
            ss >> request;
            if (request.CheckWork())
                SendMatchingCredits(request, peer);
            else
            {
                log_ << "peer " << peer->GetId() << " included bad work "
                     << "with request for matching credits\n";
                Misbehaving(peer->GetId(), 100);
            }
        }
    }

    void SendMatchingCredits(MatchingCreditsRequestMessage request,
                             CNode *peer)
    {
        MatchingCreditsMessage msg(request);
        peer->PushMessage("initdata", "matching_credits", msg);
    }

    void SendDiurnBranches(std::vector<uint160> credit_hashes, CNode *peer)
    {
        std::vector<uint160> credits;
        std::vector<std::vector<uint160> > branches;

        foreach_(const uint160& credit_hash, credit_hashes)
        {
            std::vector<uint160> branch = GetDiurnBranch(credit_hash);
            if (branch.size() == 0)
                continue;
            credits.push_back(credit_hash);
            branches.push_back(branch);
        }

        DiurnBranchMessage msg;
        msg.credit_hashes = credits;
        msg.branches = branches;

        peer->PushMessage("initdata", "diurn_branches", msg);
    }

    void SendRequestedDataMessage(DataRequestMessage request, CNode *peer)
    {
        RequestedDataMessage msg(request);
        peer->PushMessage("initdata", "requested_data", msg);
    }

    void SendInitialDataMessage(uint160 credit_hash, CNode *peer)
    {
        log_ << "SendInitialDataMessage: " << credit_hash << "\n";
        InitialDataMessage msg(credit_hash);
        log_ << "SendInitialDataMessage: sending msg\n";
        peer->PushMessage("initdata", "initial_data", msg);
    }

    void SendCalendar(uint160 credit_hash, CNode *peer)
    {
        CalendarMessage msg(credit_hash);
        log_ << "finished getting calendar for " << credit_hash << "\n"
             << "sending calendar" << msg;
        peer->PushMessage("initdata", "calendar", msg);
    }

    void SendTip(CNode *peer);
};


#endif