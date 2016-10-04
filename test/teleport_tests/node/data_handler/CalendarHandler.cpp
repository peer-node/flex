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
    if (credit_system->CalendarContainsAKnownBadCalend(calendar_message.calendar))
    {
        SendCalendarFailureMessageInResponseToCalendarMessage(calendar_message);
        return;
    }
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
        ok = ScrutinizeCalendarWithTheMostWork();
        if (not ok)
        {
            CalendarFailureDetails details = creditdata[calendar_message.GetHash160()]["failure_details"];
            CalendarFailureMessage failure_message(details);
            data_message_handler->Broadcast(failure_message);
            HandleCalendarFailureMessage(failure_message);
        }
        else
        {
            data_message_handler->calendar_handler->RequestInitialDataMessage(calendar_message);
        }
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

bool CalendarHandler::ScrutinizeCalendarWithTheMostWork()
{
    CalendarMessage calendar_message = credit_system->CalendarMessageWithMaximumReportedWork();
    CalendarFailureDetails details;

    uint160 message_hash = calendar_message.GetHash160();

    bool ok = Scrutinize(calendar_message.calendar, calendar_scrutiny_time, details);
    if (not ok)
    {
        creditdata[message_hash]["failure_details"] = details;
        creditdata[details.mined_credit_message_hash]["failure_details"] = details;
    }
    else
    {
        uint160 work = calendar_message.calendar.TotalWork();
        credit_system->RecordCalendarScrutinizedWork(calendar_message, work);
    }
    return ok;
}

bool CalendarHandler::CheckDifficultiesRootsAndProofsOfWork(Calendar &calendar)
{
    return calendar.CheckRootsAndDifficulties(credit_system) and
            calendar.CheckProofsOfWork(credit_system);
}

bool CalendarHandler::Scrutinize(Calendar calendar, uint64_t scrutiny_time, CalendarFailureDetails &details)
{
    uint64_t scrutiny_end_time = GetTimeMicros() + scrutiny_time;
    while (GetTimeMicros() < scrutiny_end_time)
    {
        if (not calendar.SpotCheckWork(details))
            return false;
    }
    return true;
}


void CalendarHandler::HandleCalendarFailureMessage(CalendarFailureMessage message)
{
    if (not credit_system->ReportedFailedCalendHasBeenReceived(message))
    {
        credit_system->RecordReportedFailureOfCalend(message);
        return;
    }
    if (not ValidateCalendarFailureMessage(message))
        return;
    MarkCalendarsAsInvalidAndSwitchToNewCalendar(message);
}

bool CalendarHandler::ValidateCalendarFailureMessage(CalendarFailureMessage failure_message)
{
    MinedCreditMessage msg = msgdata[failure_message.details.mined_credit_message_hash]["msg"];
    TwistWorkCheck &check = failure_message.details.check;
    return (bool) check.VerifyInvalid(msg.proof_of_work.proof);
}

void CalendarHandler::MarkCalendarsAsInvalidAndSwitchToNewCalendar(CalendarFailureMessage message)
{
    credit_system->MarkCalendAndSucceedingCalendsAsInvalid(message);
    credit_system->RemoveReportedTotalWorkOfMinedCreditsSucceedingInvalidCalend(message);
    if (data_message_handler->teleport_network_node != NULL)
        data_message_handler->teleport_network_node->credit_message_handler->SwitchToNewTipIfAppropriate();
}

CalendarFailureDetails CalendarHandler::GetCalendarFailureDetails(Calendar& calendar_)
{
    for (auto calend : calendar_.calends)
        if (creditdata[calend.GetHash160()].HasProperty("failure_details"))
        {
            return creditdata[calend.GetHash160()]["failure_details"];
        }
    return CalendarFailureDetails();
}

void CalendarHandler::SendCalendarFailureMessageInResponseToCalendarMessage(CalendarMessage calendar_message)
{
    CalendarFailureDetails details = GetCalendarFailureDetails(calendar_message.calendar);
    CalendarFailureMessage failure_message(details);
    CNode *peer = data_message_handler->GetPeer(calendar_message.GetHash160());
    if (peer != NULL)
        peer->PushMessage("data", "calendar_failure", failure_message);
}
