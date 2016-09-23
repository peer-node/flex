#include "DataMessageHandler.h"
#include "CalendarFailureMessage.h"


#include "CreditMessageHandler.h"
#include "FlexNetworkNode.h"

#include "log.h"
#define LOG_CATEGORY "DataMessageHandler.cpp"

using std::vector;

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
    bool requested_by_me = msgdata[request_hash]["is_tip_request"];
    CNode *peer = GetPeer(request_hash);
    TipMessage tip_message(request, calendar);
    peer->PushMessage("data", "tip", tip_message);
}

void DataMessageHandler::HandleTipMessage(TipMessage tip_message)
{
    uint160 tip_message_hash = tip_message.GetHash160();
    uint160 reported_work = tip_message.mined_credit_message.mined_credit.ReportedWork();
    if (not msgdata[tip_message.tip_request_hash]["is_tip_request"])
    {
        RejectMessage(tip_message_hash);
        return;
    }
    RecordReportedTotalWorkOfTip(tip_message_hash, reported_work);
    if (reported_work > calendar->LastMinedCreditMessage().mined_credit.ReportedWork())
    {
        RequestCalendarOfTipWithTheMostWork();
    }
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

    if (tip.mined_credit_message.mined_credit.network_state.batch_number == 0)
    {
        return 0;
    }
    uint160 tip_message_hash = tip.GetHash160();
    CNode *peer = GetPeer(tip_message_hash);
    if (peer == NULL)
        return 0;
    return SendCalendarRequestToPeer(peer, tip.mined_credit_message);
}

uint160 DataMessageHandler::SendCalendarRequestToPeer(CNode *peer, MinedCreditMessage msg)
{
    CalendarRequestMessage calendar_request(msg);
    uint160 calendar_request_hash = calendar_request.GetHash160();
    msgdata[calendar_request_hash]["is_calendar_request"] = true;
    peer->PushMessage("data", "calendar_request", calendar_request);
    msgdata[calendar_request_hash]["calendar_request"] = calendar_request;
    return calendar_request_hash;
}

TipMessage DataMessageHandler::TipWithTheMostWork()
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

std::vector<uint160> DataMessageHandler::TipMessageHashesWithTheMostReportedWork()
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
    if (credit_system->CalendarContainsAKnownBadCalend(calendar_message.calendar))
    {
        SendCalendarFailureMessageInResponseToCalendarMessage(calendar_message);
        return;
    }
    if (not ValidateCalendarMessage(calendar_message))
    {
        RejectMessage(calendar_message.GetHash160());
        return;
    }
    HandleIncomingCalendar(calendar_message);
}

void DataMessageHandler::HandleIncomingCalendar(CalendarMessage calendar_message)
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
            Broadcast(failure_message);
            HandleCalendarFailureMessage(failure_message);
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

bool DataMessageHandler::ScrutinizeCalendarWithTheMostWork()
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
    if (not ValidateInitialDataMessage(initial_data_message))
    {
        return;
    }
    auto requested_calendar = GetRequestedCalendar(initial_data_message);
    if (requested_calendar.LastMinedCreditMessage().mined_credit.ReportedWork() >
            calendar->LastMinedCreditMessage().mined_credit.ReportedWork())
        UseInitialDataMessageAndCalendar(initial_data_message, requested_calendar);
}

void DataMessageHandler::UseInitialDataMessageAndCalendar(InitialDataMessage initial_data_message,
                                                          Calendar new_calendar)
{
    StoreDataFromInitialDataMessageInCreditSystem(initial_data_message, *credit_system);
    MarkMinedCreditMessagesInInitialDataMessageAsValidated(initial_data_message);
    flex_network_node->SwitchToNewCalendarAndSpentChain(new_calendar, initial_data_message.spent_chain);
}

void DataMessageHandler::MarkMinedCreditMessagesInInitialDataMessageAsValidated(InitialDataMessage initial_data_message)
{
    for (auto msg : initial_data_message.mined_credit_messages_in_current_diurn)
    {
        uint160 msg_hash = msg.GetHash160();
        creditdata[msg_hash]["passed_verification"] = true;
    }
}

bool DataMessageHandler::ValidateInitialDataMessage(InitialDataMessage initial_data_message)
{
    if (not EnclosedMessagesArePresentInInitialDataMessage(initial_data_message))
    {
        log_ << "initial data message doesn't contain necessary data\n";
        return false;
    }

    if (not CheckSpentChainInInitialDataMessage(initial_data_message))
    {
        log_ << "bad spent chain included in initial data message\n";
        return false;
    }

    if (not InitialDataMessageMatchesRequestedCalendar(initial_data_message))
    {
        log_ << "initial data message doesn't match calendar\n";
        return false;
    }

    if (not ValidateMinedCreditMessagesInInitialDataMessage(initial_data_message))
    {
        log_ << "mined credit messages in initial data message aren't good\n";
        return false;
    }

    return true;
}

bool DataMessageHandler::CheckSpentChainInInitialDataMessage(InitialDataMessage initial_data_message)
{
    Calend calend = initial_data_message.GetLastCalend();
    return calend.mined_credit.network_state.spent_chain_hash == initial_data_message.spent_chain.GetHash160();
}

void LoadHashesIntoDataStoreFromMessageTypesAndContents(MemoryDataStore &hashdata,
                                                        std::vector<std::string> &types,
                                                        std::vector<vch_t> &contents,
                                                        CreditSystem *credit_system)
{
    for (uint32_t i = 0; i < types.size(); i++)
    {
        uint160 enclosed_message_hash;
        if (types[i] != "msg")
            enclosed_message_hash = Hash160(contents[i].begin(), contents[i].end());
        else
        {
            CDataStream ss(contents[i], SER_NETWORK, CLIENT_VERSION);
            MinedCreditMessage msg;
            ss >> msg;
            enclosed_message_hash = msg.GetHash160();
        }
        credit_system->StoreHash(enclosed_message_hash, hashdata);
    }
}

MemoryDataStore DataMessageHandler::GetEnclosedMessageHashes(InitialDataMessage &message)
{
    MemoryDataStore hashdata;
    for (auto mined_credit_message : message.mined_credit_messages_in_current_diurn)
    {
        credit_system->StoreHash(mined_credit_message.GetHash160(), hashdata);
    }
    LoadHashesIntoDataStoreFromMessageTypesAndContents(hashdata,
                                                       message.enclosed_message_types,
                                                       message.enclosed_message_contents, credit_system);
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
            log_ << "entries in diurn don't match\n";
            return false;
        }
    }
    else if (calendar.current_diurn.Size() == 1)
    {
        auto enclosed_messages = data_message.mined_credit_messages_in_current_diurn;

        if (not SequenceOfMinedCreditMessagesIsValidAndHasValidProofsOfWork(enclosed_messages))
            return false;
        if (enclosed_messages.size() == 0 or enclosed_messages.back() != calendar.current_diurn.credits_in_diurn[0])
            return false;
        if (calendar.calends.size() > 1 and enclosed_messages.front() != calendar.calends[calendar.calends.size() - 2])
            return false;
    }
    else if (calendar.current_diurn.Size() == 0)
        return false;

    return true;
}

bool DataMessageHandler::SequenceOfMinedCreditMessagesIsValidAndHasValidProofsOfWork(vector<MinedCreditMessage> msgs)
{
    return SequenceOfMinedCreditMessagesIsValid(msgs) and SequenceOfMinedCreditMessagesHasValidProofsOfWork(msgs);
}

bool DataMessageHandler::SequenceOfMinedCreditMessagesIsValid(std::vector<MinedCreditMessage> msgs)
{
    return Calendar::CheckCreditMessageHashesInSequenceOfConsecutiveMinedCreditMessages(msgs) and
            Calendar::CheckDifficultiesOfConsecutiveSequenceOfMinedCreditMessages(msgs);
}

void DataMessageHandler::StoreDataFromInitialDataMessageInCreditSystem(InitialDataMessage &initial_data_message,
                                                                       CreditSystem &credit_system)
{
    uint160 calend_hash = initial_data_message.mined_credit_messages_in_current_diurn[0].GetHash160();
    credit_system.creditdata[calend_hash]["first_in_data_message"] = true;

    Calendar calendar = GetRequestedCalendar(initial_data_message);

    for (auto calend : calendar.calends)
        credit_system.StoreMinedCreditMessage(calend);

    for (auto mined_credit_message : initial_data_message.mined_credit_messages_in_current_diurn)
        credit_system.StoreMinedCreditMessage(mined_credit_message);

    for (uint32_t i = 0; i < initial_data_message.enclosed_message_types.size(); i++)
    {
        std::string type = initial_data_message.enclosed_message_types[i];
        vch_t content = initial_data_message.enclosed_message_contents[i];
        StoreMessageInCreditSystem(type, content, credit_system);
    }
}

void DataMessageHandler::StoreMessageInCreditSystem(std::string type, vch_t content, CreditSystem &credit_system)
{
    if (type == "msg")
    {
        CDataStream ss(content, SER_NETWORK, CLIENT_VERSION);
        MinedCreditMessage msg;
        ss >> msg;
        credit_system.StoreMinedCreditMessage(msg);
    }
    else if (type == "tx")
    {
        CDataStream ss(content, SER_NETWORK, CLIENT_VERSION);
        SignedTransaction tx;
        ss >> tx;
        credit_system.StoreTransaction(tx);
    }
}

bool DataMessageHandler::SequenceOfMinedCreditMessagesHasValidProofsOfWork(std::vector<MinedCreditMessage> msgs)
{
    for (auto mined_credit_message : msgs)
    {
        if (not credit_system->QuickCheckProofOfWorkInMinedCreditMessage(mined_credit_message))
            return false;
    }
    return true;
}

void DataMessageHandler::SetMiningParametersForInitialDataMessageValidation(uint64_t number_of_megabytes_,
                                                                            uint160 initial_difficulty_,
                                                                            uint160 initial_diurnal_difficulty_)
{
    number_of_megabytes_for_mining = number_of_megabytes_;
    initial_difficulty = initial_difficulty_;
    initial_diurnal_difficulty = initial_diurnal_difficulty_;
}

bool DataMessageHandler::ValidateMinedCreditMessagesInInitialDataMessage(InitialDataMessage initial_data_message)
{
    MemoryDataStore msgdata_, creditdata_, keydata_;
    CreditSystem credit_system_(msgdata_, creditdata_);

    credit_system_.SetMiningParameters(number_of_megabytes_for_mining, initial_difficulty, initial_diurnal_difficulty);
    StoreDataFromInitialDataMessageInCreditSystem(initial_data_message, credit_system_);

    CreditMessageHandler credit_message_handler(msgdata_, creditdata_, keydata_);
    credit_message_handler.SetCreditSystem(&credit_system_);

    BitChain spent_chain = initial_data_message.spent_chain;
    credit_message_handler.SetSpentChain(spent_chain);

    Calendar validation_calendar = GetRequestedCalendar(initial_data_message);
    TrimLastDiurnFromCalendar(validation_calendar, &credit_system_);
    credit_message_handler.SetCalendar(validation_calendar);

    for (auto mined_credit_message : initial_data_message.mined_credit_messages_in_current_diurn)
        credit_message_handler.Handle(mined_credit_message, NULL);

    return TipOfCalendarMatchesTipOfInitialDataMessage(validation_calendar, initial_data_message);
}

bool DataMessageHandler::TipOfCalendarMatchesTipOfInitialDataMessage(Calendar &calendar, InitialDataMessage &message)
{
    return calendar.current_diurn.credits_in_diurn.back() == message.mined_credit_messages_in_current_diurn.back();
}

void DataMessageHandler::TrimLastDiurnFromCalendar(Calendar& calendar, CreditSystem *credit_system)
{
    if (calendar.current_diurn.Size() == 1 and calendar.calends.size() > 0)
        calendar.RemoveLast(credit_system);

    while (calendar.current_diurn.Size() > 1)
        calendar.RemoveLast(credit_system);
}

void DataMessageHandler::SetNetworkNode(FlexNetworkNode *flex_network_node_)
{
    flex_network_node = flex_network_node_;
}

void DataMessageHandler::HandleCalendarFailureMessage(CalendarFailureMessage message)
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

bool DataMessageHandler::ValidateCalendarFailureMessage(CalendarFailureMessage message)
{
    MinedCreditMessage msg = msgdata[message.details.mined_credit_message_hash]["msg"];
    TwistWorkCheck &check = message.details.check;
    return (bool) check.VerifyInvalid(msg.proof_of_work.proof);
}

void DataMessageHandler::MarkCalendarsAsInvalidAndSwitchToNewCalendar(CalendarFailureMessage message)
{
    credit_system->MarkCalendAndSucceedingCalendsAsInvalid(message);
    credit_system->RemoveReportedTotalWorkOfMinedCreditsSucceedingInvalidCalend(message);
    if (flex_network_node != NULL)
        flex_network_node->credit_message_handler->SwitchToNewTipIfAppropriate();
}

CalendarFailureDetails DataMessageHandler::GetCalendarFailureDetails(Calendar& calendar_)
{
    for (auto calend : calendar_.calends)
        if (creditdata[calend.GetHash160()].HasProperty("failure_details"))
        {
            return creditdata[calend.GetHash160()]["failure_details"];
        }
    return CalendarFailureDetails();
}

void DataMessageHandler::SendCalendarFailureMessageInResponseToCalendarMessage(CalendarMessage calendar_message)
{
    CalendarFailureDetails details = GetCalendarFailureDetails(calendar_message.calendar);
    CalendarFailureMessage failure_message(details);
    CNode *peer = GetPeer(calendar_message.GetHash160());
    if (peer != NULL)
        peer->PushMessage("data", "calendar_failure", failure_message);
}
