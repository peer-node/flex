#include "InitialDataHandler.h"
#include "CalendarHandler.h"
#include "test/teleport_tests/node/historical_data/LoadHashesIntoDataStore.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"

#include "log.h"
#define LOG_CATEGORY "InitialDataHandler.cpp"

void InitialDataHandler::HandleInitialDataRequestMessage(InitialDataRequestMessage request)
{
    InitialDataMessage initial_data_message(request, credit_system);
    CNode *peer = data_message_handler->GetPeer(request.GetHash160());
    peer->PushMessage("data", "initial_data", initial_data_message);
}

void InitialDataHandler::HandleInitialDataMessage(InitialDataMessage initial_data_message)
{
    uint160 message_hash = initial_data_message.GetHash160();
    if (not msgdata[initial_data_message.request_hash]["is_data_request"])
    {
        data_message_handler->RejectMessage(message_hash);
        return;
    }
    if (not ValidateInitialDataMessage(initial_data_message))
    {
        return;
    }
    auto requested_calendar = credit_system->GetRequestedCalendar(initial_data_message);
    if (requested_calendar.LastMinedCreditMessage().mined_credit.ReportedWork() >
            data_message_handler->calendar->LastMinedCreditMessage().mined_credit.ReportedWork())
        UseInitialDataMessageAndCalendar(initial_data_message, requested_calendar);
}

void InitialDataHandler::UseInitialDataMessageAndCalendar(InitialDataMessage initial_data_message,
                                                          Calendar new_calendar)
{
    StoreDataFromInitialDataMessageInCreditSystem(initial_data_message, *credit_system);
    MarkMinedCreditMessagesInInitialDataMessageAsValidated(initial_data_message);
    auto tip = initial_data_message.mined_credit_messages_in_current_diurn.back();

    RelayState state = data_message_handler->data.GetRelayState(tip.mined_credit.network_state.relay_state_hash);
    state.latest_mined_credit_message_hash = tip.GetHash160();

    data_message_handler->teleport_network_node->SwitchToNewCalendarSpentChainAndRelayState(new_calendar,
                                                                                            initial_data_message.spent_chain,
                                                                                            state);
}

void InitialDataHandler::MarkMinedCreditMessagesInInitialDataMessageAsValidated(InitialDataMessage initial_data_message)
{
    for (auto msg : initial_data_message.mined_credit_messages_in_current_diurn)
    {
        uint160 msg_hash = msg.GetHash160();
        creditdata[msg_hash]["passed_verification"] = true;
    }
}

bool InitialDataHandler::ValidateInitialDataMessage(InitialDataMessage initial_data_message)
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

bool InitialDataHandler::CheckSpentChainInInitialDataMessage(InitialDataMessage initial_data_message)
{
    Calend calend = initial_data_message.GetLastCalend();
    return calend.mined_credit.network_state.spent_chain_hash == initial_data_message.spent_chain.GetHash160();
}

MemoryDataStore InitialDataHandler::GetEnclosedMessageHashes(InitialDataMessage &message)
{
    MemoryDataStore hashdata;
    for (auto mined_credit_message : message.mined_credit_messages_in_current_diurn)
    {
        log_ << "GetEnclosedMessageHashes: storing " << mined_credit_message.GetHash160() << " from current diurn\n";
        credit_system->StoreHash(mined_credit_message.GetHash160(), hashdata);
    }
    LoadHashesIntoDataStoreFromMessageTypesAndContents(hashdata,
                                                       message.message_data.enclosed_message_types,
                                                       message.message_data.enclosed_message_contents,
                                                       credit_system);
    return hashdata;
}

bool InitialDataHandler::EnclosedMessagesArePresentInInitialDataMessage(InitialDataMessage &message)
{
    MemoryDataStore enclosed_message_hashes = GetEnclosedMessageHashes(message);
    for (auto mined_credit_message : message.mined_credit_messages_in_current_diurn)
    {
        if (not mined_credit_message.hash_list.RecoverFullHashes(enclosed_message_hashes))
        {
            log_ << "failed to recover full hashes from " << mined_credit_message.GetHash160()
                 << " specifically " << mined_credit_message.hash_list.HexShortHashes() << "\n";
            return false;
        }
    }
    return true;
}

bool InitialDataHandler::InitialDataMessageMatchesRequestedCalendar(InitialDataMessage &initial_data_message)
{
    Calendar requested_calendar = credit_system->GetRequestedCalendar(initial_data_message);
    return InitialDataMessageMatchesCalendar(initial_data_message, requested_calendar);
}


bool InitialDataHandler::InitialDataMessageMatchesCalendar(InitialDataMessage &data_message, Calendar calendar)
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

bool InitialDataHandler::SequenceOfMinedCreditMessagesIsValidAndHasValidProofsOfWork(std::vector<MinedCreditMessage> msgs)
{
    return SequenceOfMinedCreditMessagesIsValid(msgs) and SequenceOfMinedCreditMessagesHasValidProofsOfWork(msgs);
}

bool InitialDataHandler::SequenceOfMinedCreditMessagesIsValid(std::vector<MinedCreditMessage> msgs)
{
    return Calendar::CheckCreditMessageHashesInSequenceOfConsecutiveMinedCreditMessages(msgs) and
           Calendar::CheckDifficultiesOfConsecutiveSequenceOfMinedCreditMessages(msgs);
}

void InitialDataHandler::StoreDataFromInitialDataMessageInCreditSystem(InitialDataMessage &initial_data_message,
                                                                       CreditSystem &credit_system_)
{
    uint160 calend_hash = initial_data_message.mined_credit_messages_in_current_diurn[0].GetHash160();
    credit_system_.creditdata[calend_hash]["first_in_data_message"] = true;

    Calendar calendar = credit_system->GetRequestedCalendar(initial_data_message);

    for (auto calend : calendar.calends)
        credit_system_.StoreMinedCreditMessage(calend);

    for (auto mined_credit_message : initial_data_message.mined_credit_messages_in_current_diurn)
        credit_system_.StoreMinedCreditMessage(mined_credit_message);

    credit_system_.StoreEnclosedMessages(initial_data_message.message_data);
}

bool InitialDataHandler::SequenceOfMinedCreditMessagesHasValidProofsOfWork(std::vector<MinedCreditMessage> msgs)
{
    for (auto mined_credit_message : msgs)
    {
        if (not credit_system->QuickCheckProofOfWorkInMinedCreditMessage(mined_credit_message))
            return false;
    }
    return true;
}

void InitialDataHandler::SetMiningParametersForInitialDataMessageValidation(uint64_t number_of_megabytes_,
                                                                            uint160 initial_difficulty_,
                                                                            uint160 initial_diurnal_difficulty_)
{
}

bool InitialDataHandler::ValidateMinedCreditMessagesInInitialDataMessage(InitialDataMessage initial_data_message)
{
    TeleportNetworkNode validation_node;
    validation_node.credit_system->SetMiningParameters(credit_system->initial_difficulty,
                                                       credit_system->initial_diurnal_difficulty);
    StoreDataFromInitialDataMessageInCreditSystem(initial_data_message, *validation_node.credit_system);

    validation_node.spent_chain = initial_data_message.spent_chain;
    log_ << "set spent chain to that of initial data message. hash = " << validation_node.spent_chain.GetHash160() << "\n";
    log_ << "initial dta message has spent chain: " << validation_node.spent_chain.ToString() << "n";
    validation_node.data.creditdata[initial_data_message.GetLastCalend().GetHash160()]["spent_chain"] = validation_node.spent_chain;

    validation_node.calendar = credit_system->GetRequestedCalendar(initial_data_message);
    TrimLastDiurnFromCalendar(validation_node.calendar, validation_node.credit_system);
    validation_node.credit_message_handler->SetCalendar(validation_node.calendar);

    for (auto mined_credit_message : initial_data_message.mined_credit_messages_in_current_diurn)
    {
        auto msg_hash = mined_credit_message.GetHash160();
        log_ << "trying to validate msg: " << msg_hash << "\n";
        log_ << "enclosed messages are present: "
             << mined_credit_message.hash_list.RecoverFullHashes(validation_node.data.msgdata) << "\n";
        validation_node.credit_system->MarkMinedCreditMessageAsHandled(mined_credit_message.mined_credit.network_state.previous_mined_credit_message_hash);
        validation_node.HandleMessage(msg_hash);
        if (validation_node.Tip().GetHash160() == msg_hash)
        {
            RelayState state = validation_node.data.GetRelayState(mined_credit_message.mined_credit.network_state.relay_state_hash);
            data_message_handler->data.StoreRelayState(&state);
        }
    }

    return TipOfCalendarMatchesTipOfInitialDataMessage(validation_node.calendar, initial_data_message);
}

bool InitialDataHandler::TipOfCalendarMatchesTipOfInitialDataMessage(Calendar &calendar, InitialDataMessage &message)
{
    return calendar.current_diurn.credits_in_diurn.back() == message.mined_credit_messages_in_current_diurn.back();
}

void InitialDataHandler::TrimLastDiurnFromCalendar(Calendar& calendar, CreditSystem *credit_system)
{
    if (calendar.current_diurn.Size() == 1 and calendar.calends.size() > 0)
        calendar.RemoveLast(credit_system);

    while (calendar.current_diurn.Size() > 1)
        calendar.RemoveLast(credit_system);
}
