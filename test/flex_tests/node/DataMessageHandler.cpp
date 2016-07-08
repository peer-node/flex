#include "DataMessageHandler.h"
#include "CalendarRequestMessage.h"
#include "CalendarMessage.h"

#include "log.h"
#define LOG_CATEGORY "DataMessageHandler.cpp"


void DataMessageHandler::SetCreditSystem(CreditSystem *credit_system_)
{
    credit_system = credit_system_;
}

void DataMessageHandler::SetCalendar(Calendar *calendar_)
{
    calendar = calendar_;
}

void DataMessageHandler::HandleTipRequestMessage(TipRequestMessage request)
{
    uint160 request_hash = request.GetHash160();
    CNode *peer = GetPeer(request_hash);
    TipMessage tip_message(request, calendar);
    peer->PushMessage("data", "tip", tip_message);
}

void DataMessageHandler::HandleTipMessage(TipMessage tip_message)
{
    uint160 tip_message_hash = tip_message.GetHash160();
    if (not msgdata[tip_message.tip_request_hash]["is_tip_request"])
    {
        RejectMessage(tip_message_hash);
        return;
    }
    RecordReportedTotalWorkOfTip(tip_message_hash, tip_message.mined_credit.ReportedWork());
}

void DataMessageHandler::RecordReportedTotalWorkOfTip(uint160 tip_message_hash, uint160 total_work)
{
    std::vector<uint160> existing_hashes;
    creditdata.GetObjectAtLocation(existing_hashes, "reported_work", total_work);
    if (not VectorContainsEntry(existing_hashes, tip_message_hash))
        existing_hashes.push_back(tip_message_hash);
    creditdata[existing_hashes].Location("reported_work") = total_work;
}

uint160 DataMessageHandler::RequestTips()
{
    TipRequestMessage tip_request;
    uint160 tip_request_hash = tip_request.GetHash160();
    msgdata[tip_request_hash]["is_tip_request"] =  true;
    msgdata[tip_request_hash]["tip_request"] = tip_request;
    Broadcast(tip_request);
    return tip_request_hash;
}

uint160 DataMessageHandler::RequestCalendarOfTipWithTheMostWork()
{
    TipMessage tip = TipWithTheMostWork();
    uint160 tip_message_hash = tip.GetHash160();
    CNode *peer = GetPeer(tip_message_hash);
    if (peer == NULL)
        return 0;
    return SendCalendarRequestToPeer(peer, tip.mined_credit);
}

uint160 DataMessageHandler::SendCalendarRequestToPeer(CNode *peer, MinedCredit mined_credit)
{
    CalendarRequestMessage calendar_request(mined_credit);
    uint160 calendar_request_hash = calendar_request.GetHash160();
    msgdata[calendar_request_hash]["is_calendar_request"] = true;
    peer->PushMessage("data", "calendar_request", calendar_request);
    msgdata[calendar_request_hash]["calendar_request"] = calendar_request;
    return calendar_request_hash;
}

TipMessage DataMessageHandler::TipWithTheMostWork()
{
    TipMessage tip_message;
    auto tip_message_hashes_with_the_most_work = TipMessageHashesWithTheMostWork();
    if (tip_message_hashes_with_the_most_work.size() == 0)
        return tip_message;
    tip_message = msgdata[tip_message_hashes_with_the_most_work[0]]["tip"];
    return tip_message;
}

std::vector<uint160> DataMessageHandler::TipMessageHashesWithTheMostWork()
{
    std::vector<uint160> tip_message_hashes;
    LocationIterator work_scanner = creditdata.LocationIterator("reported_work");
    work_scanner.SeekEnd();
    uint160 total_work(0);
    work_scanner.GetPreviousObjectAndLocation(tip_message_hashes, total_work);
    return tip_message_hashes;
}

void DataMessageHandler::HandleCalendarRequestMessage(CalendarRequestMessage request)
{
    CalendarMessage calendar_message(request, credit_system);
    CNode *peer = GetPeer(request.GetHash160());
    if (peer != NULL)
        SendMessageToPeer(calendar_message, peer);
}

void DataMessageHandler::HandleCalendarMessage(CalendarMessage calendar_message)
{
    if (not msgdata[calendar_message.request_hash]["is_calendar_request"])
    {
        RejectMessage(calendar_message.GetHash160());
        return;
    }
}
