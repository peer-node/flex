#include "DataMessageHandler.h"
#include "CalendarFailureMessage.h"

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

    if (tip.mined_credit.network_state.batch_number == 0)
    {
        return 0;
    }
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
    {
        return tip_message;
    }
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

bool DataMessageHandler::ValidateCalendarMessage(CalendarMessage calendar_message)
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

void DataMessageHandler::HandleCalendarMessage(CalendarMessage calendar_message)
{
    if (not ValidateCalendarMessage(calendar_message))
    {
        RejectMessage(calendar_message.GetHash160());
        return;
    }
    HandleIncomingCalendar(calendar_message);
}

void DataMessageHandler::HandleIncomingCalendar(CalendarMessage calendar_message)
{
    uint160 reported_work = calendar_message.calendar.LastMinedCredit().ReportedWork();
    credit_system->RecordCalendarReportedWork(calendar_message, reported_work);
    uint160 maximum_reported_work = credit_system->MaximumReportedCalendarWork();

    bool ok = false;
    if (maximum_reported_work == reported_work)
    {
        ok = ScrutinizeCalendarWithTheMostWork();
        if (not ok)
        {
            CalendarFailureDetails details = creditdata[calendar_message.GetHash160()]["failure_details"];
            CalendarFailureMessage failure_message(details);

            Broadcast(failure_message);
        }
        else
        {
            RequestInitialDataMessage(calendar_message);
        }
    }
}

void DataMessageHandler::RequestInitialDataMessage(CalendarMessage calendar_message)
{

    InitialDataRequestMessage request(calendar_message);

    uint160 request_hash = request.GetHash160();
    uint160 calendar_message_hash = calendar_message.GetHash160();

    msgdata[request_hash]["is_data_request"] = true;
    msgdata[request_hash]["calendar_message_hash"] = calendar_message_hash;

    CNode *peer = GetPeer(calendar_message_hash);
    if (peer != NULL)
        peer->PushMessage("data", "initial_data_request", request);
}

void DataMessageHandler::SwitchToCalendarWithTheMostWorkWhichHasSurvivedScrutiny()
{
    Calendar calendar_with_most_scrutinized_work = CalendarWithTheMostWorkWhichHasSurvivedScrutiny();
    SwitchToCalendar(calendar_with_most_scrutinized_work);
}

Calendar DataMessageHandler::CalendarWithTheMostWorkWhichHasSurvivedScrutiny()
{
    CalendarMessage calendar_message = credit_system->CalendarMessageWithMaximumScrutinizedWork();
    return calendar_message.calendar;
}

void DataMessageHandler::SwitchToCalendar(Calendar new_calendar)
{
    *calendar = new_calendar;
}

bool DataMessageHandler::ScrutinizeCalendarWithTheMostWork()
{
    CalendarMessage calendar_message = credit_system->CalendarMessageWithMaximumReportedWork();
    CalendarFailureDetails details;

    uint160 message_hash = calendar_message.GetHash160();

    bool ok = Scrutinize(calendar_message.calendar, calendar_scrutiny_time, details);
    if (not ok)
    {
        creditdata[message_hash]["failure_details"] = details;
    }
    else
    {
        uint160 work = calendar_message.calendar.TotalWork();
        credit_system->RecordCalendarScrutinizedWork(calendar_message, work);
    }
    return ok;
}

bool DataMessageHandler::CheckDifficultiesRootsAndProofsOfWork(Calendar &calendar)
{
    return calendar.CheckRootsAndDifficulties(credit_system) and calendar.CheckProofsOfWork(credit_system);
}

bool DataMessageHandler::Scrutinize(Calendar calendar, uint64_t scrutiny_time, CalendarFailureDetails &details)
{
    uint64_t scrutiny_end_time = GetTimeMicros() + scrutiny_time;
    while (GetTimeMicros() < scrutiny_end_time)
    {
        if (not calendar.SpotCheckWork(details))
            return false;
    }
    return true;
}

void DataMessageHandler::HandleInitialDataRequestMessage(InitialDataRequestMessage request)
{
    InitialDataMessage initial_data_message(request, credit_system);
    CNode *peer = GetPeer(request.GetHash160());
    peer->PushMessage("data", "initial_data", initial_data_message);
}

void DataMessageHandler::HandleInitialDataMessage(InitialDataMessage initial_data_message)
{
    uint160 message_hash = initial_data_message.GetHash160();
    if (not msgdata[initial_data_message.request_hash]["is_data_request"])
    {
        RejectMessage(message_hash);
        return;
    }
}

bool DataMessageHandler::CheckSpentChainInInitialDataMessage(InitialDataMessage initial_data_message)
{
    Calend calend = initial_data_message.GetLastCalend();
    return calend.mined_credit.network_state.spent_chain_hash == initial_data_message.spent_chain.GetHash160();
}

MemoryDataStore DataMessageHandler::GetEnclosedMessageHashes(InitialDataMessage &message)
{
    MemoryDataStore hashdata;
    for (auto mined_credit_message : message.mined_credit_messages_in_current_diurn)
    {
        credit_system->StoreHash(mined_credit_message.mined_credit.GetHash160(), hashdata);
    }
    for (uint32_t i = 0; i < message.enclosed_message_types.size(); i++)
    {
        std::string type = message.enclosed_message_types[i];
        vch_t content = message.enclosed_message_contents[i];

        uint160 enclosed_message_hash;
        if (type != "mined_credit")
            enclosed_message_hash = Hash160(content.begin(), content.end());
        else
        {
            CDataStream ss(content, SER_NETWORK, CLIENT_VERSION);
            MinedCredit mined_credit;
            ss >> mined_credit;
            enclosed_message_hash = mined_credit.GetHash160();
        }
        credit_system->StoreHash(enclosed_message_hash, hashdata);
    }
    return hashdata;
}

bool DataMessageHandler::EnclosedMessagesArePresentInInitialDataMessage(InitialDataMessage &message)
{
    MemoryDataStore enclosed_message_hashes = GetEnclosedMessageHashes(message);
    for (auto mined_credit_message : message.mined_credit_messages_in_current_diurn)
    {
        if (not mined_credit_message.hash_list.RecoverFullHashes(enclosed_message_hashes))
            return false;
    }
    return true;
}

bool DataMessageHandler::InitialDataMessageMatchesRequestedCalendar(InitialDataMessage &initial_data_message)
{
    Calendar requested_calendar = GetRequestedCalendar(initial_data_message);
    return InitialDataMessageMatchesCalendar(initial_data_message, requested_calendar);
}

Calendar DataMessageHandler::GetRequestedCalendar(InitialDataMessage &initial_data_message)
{
    uint160 request_hash = initial_data_message.request_hash;
    uint160 calendar_message_hash = msgdata[request_hash]["calendar_message_hash"];
    CalendarMessage calendar_message = msgdata[calendar_message_hash]["calendar"];
    return calendar_message.calendar;
}

bool DataMessageHandler::InitialDataMessageMatchesCalendar(InitialDataMessage &data_message, Calendar calendar)
{
    if (calendar.current_diurn.Size() > 1 or calendar.calends.size() == 0)
    {
        if (data_message.mined_credit_messages_in_current_diurn != calendar.current_diurn.credits_in_diurn)
        {
            return false;
        }
    }
    else if (calendar.current_diurn.Size() == 1)
    {
        if (data_message.mined_credit_messages_in_current_diurn.back() != calendar.current_diurn.credits_in_diurn[0])
            return false;
    }
    else if (calendar.current_diurn.Size() == 0)
        return false;

    return true;
}
