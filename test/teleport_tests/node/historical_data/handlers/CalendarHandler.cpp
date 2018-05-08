#include "CalendarHandler.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"


#include "log.h"
#define LOG_CATEGORY "CalendarHandler.cpp"

void CalendarHandler::HandleCalendarRequestMessage(CalendarRequestMessage request)
{
    CalendarMessage calendar_message(request, credit_system);
    CNode *peer = data_message_handler->GetPeer(request.GetHash160());
    if (peer != NULL)
        data_message_handler->SendMessageToPeer(calendar_message, peer);
}

bool CalendarHandler::ValidateCalendarMessage(CalendarMessage calendar_message)
{
    if (not msgdata[calendar_message.request_hash]["is_calendar_request"])
    {
        return false;
    }
    else if (not calendar_message.calendar.CheckRootsAndDifficulties(credit_system))
    {
        return false;
    }
    return true;
}

void CalendarHandler::HandleCalendarMessage(CalendarMessage calendar_message)
{
    if (not ValidateCalendarMessage(calendar_message))
    {
        log_ << "Failed to validate\n";
        data_message_handler->RejectMessage(calendar_message.GetHash160());
        return;
    }
    HandleIncomingCalendar(calendar_message);
}

void CalendarHandler::HandleIncomingCalendar(CalendarMessage calendar_message)
{
    uint160 reported_work = calendar_message.calendar.LastMinedCreditMessage().mined_credit.ReportedWork();
    credit_system->RecordCalendarReportedWork(calendar_message, reported_work);
    credit_system->StoreCalendsFromCalendar(calendar_message.calendar);

    uint160 maximum_reported_work = credit_system->MaximumReportedCalendarWork();

    bool ok = false;
    if (maximum_reported_work == reported_work)
    {
        data_message_handler->calendar_handler->RequestInitialDataMessage(calendar_message);
    }
}


void CalendarHandler::RequestInitialDataMessage(CalendarMessage calendar_message)
{
    InitialDataRequestMessage request(calendar_message);

    uint160 request_hash = request.GetHash160();
    uint160 calendar_message_hash = calendar_message.GetHash160();

    msgdata[request_hash]["is_data_request"] = true;
    msgdata[request_hash]["calendar_message_hash"] = calendar_message_hash;

    CNode *peer = data_message_handler->GetPeer(calendar_message_hash);
    if (peer != NULL)
        peer->PushMessage("data", "initial_data_request", request);
}

bool CalendarHandler::CheckDifficultiesRootsAndProofsOfWork(Calendar &calendar)
{
    return calendar.CheckRootsAndDifficulties(credit_system) and
            calendar.CheckProofsOfWork(credit_system);
}
