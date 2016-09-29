#include "TipHandler.h"


void TipHandler::HandleTipRequestMessage(TipRequestMessage request)
{
    uint160 request_hash = request.GetHash160();
    bool requested_by_me = msgdata[request_hash]["is_tip_request"];
    CNode *peer = data_message_handler->GetPeer(request_hash);
    TipMessage tip_message(request, data_message_handler->calendar);
    peer->PushMessage("data", "tip", tip_message);
}

void TipHandler::HandleTipMessage(TipMessage tip_message)
{
    uint160 tip_message_hash = tip_message.GetHash160();
    uint160 reported_work = tip_message.mined_credit_message.mined_credit.ReportedWork();
    if (not msgdata[tip_message.tip_request_hash]["is_tip_request"])
    {
        data_message_handler->RejectMessage(tip_message_hash);
        return;
    }
    RecordReportedTotalWorkOfTip(tip_message_hash, reported_work);
    if (reported_work > data_message_handler->calendar->LastMinedCreditMessage().mined_credit.ReportedWork())
    {
        RequestCalendarOfTipWithTheMostWork();
    }
}

void TipHandler::RecordReportedTotalWorkOfTip(uint160 tip_message_hash, uint160 total_work)
{
    std::vector<uint160> existing_hashes;
    creditdata.GetObjectAtLocation(existing_hashes, "reported_work", total_work);
    if (not VectorContainsEntry(existing_hashes, tip_message_hash))
        existing_hashes.push_back(tip_message_hash);
    creditdata[existing_hashes].Location("reported_work") = total_work;
}

uint160 TipHandler::RequestTips()
{
    TipRequestMessage tip_request;
    uint160 tip_request_hash = tip_request.GetHash160();
    msgdata[tip_request_hash]["is_tip_request"] =  true;
    msgdata[tip_request_hash]["tip_request"] = tip_request;
    data_message_handler->Broadcast(tip_request);
    return tip_request_hash;
}

uint160 TipHandler::RequestCalendarOfTipWithTheMostWork()
{
    TipMessage tip = TipWithTheMostWork();

    if (tip.mined_credit_message.mined_credit.network_state.batch_number == 0)
    {
        return 0;
    }
    uint160 tip_message_hash = tip.GetHash160();
    CNode *peer = data_message_handler->GetPeer(tip_message_hash);
    if (peer == NULL)
        return 0;
    return SendCalendarRequestToPeer(peer, tip.mined_credit_message);
}

uint160 TipHandler::SendCalendarRequestToPeer(CNode *peer, MinedCreditMessage msg)
{
    CalendarRequestMessage calendar_request(msg);
    uint160 calendar_request_hash = calendar_request.GetHash160();
    msgdata[calendar_request_hash]["is_calendar_request"] = true;
    peer->PushMessage("data", "calendar_request", calendar_request);
    msgdata[calendar_request_hash]["calendar_request"] = calendar_request;
    return calendar_request_hash;
}

TipMessage TipHandler::TipWithTheMostWork()
{
    TipMessage tip_message;
    auto tip_message_hashes_with_the_most_work = TipMessageHashesWithTheMostReportedWork();
    if (tip_message_hashes_with_the_most_work.size() == 0)
    {
        return tip_message;
    }
    tip_message = msgdata[tip_message_hashes_with_the_most_work[0]]["tip"];

    return tip_message;
}

std::vector<uint160> TipHandler::TipMessageHashesWithTheMostReportedWork()
{
    std::vector<uint160> tip_message_hashes;
    LocationIterator work_scanner = creditdata.LocationIterator("reported_work");
    work_scanner.SeekEnd();
    uint160 total_work(0);
    work_scanner.GetPreviousObjectAndLocation(tip_message_hashes, total_work);
    return tip_message_hashes;
}
