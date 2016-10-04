#include "InitialDataHandler.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"
#include "CalendarHandler.h"

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
    data_message_handler->teleport_network_node->SwitchToNewCalendarAndSpentChain(new_calendar, initial_data_message.spent_chain);
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

MemoryDataStore InitialDataHandler::GetEnclosedMessageHashes(InitialDataMessage &message)
{
    MemoryDataStore hashdata;
    for (auto mined_credit_message : message.mined_credit_messages_in_current_diurn)
    {
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
            return false;
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
    data_message_handler->calendar_handler->number_of_megabytes_for_mining = number_of_megabytes_;
    initial_difficulty = initial_difficulty_;
    initial_diurnal_difficulty = initial_diurnal_difficulty_;
}

bool InitialDataHandler::ValidateMinedCreditMessagesInInitialDataMessage(InitialDataMessage initial_data_message)
{
    MemoryDataStore msgdata_, creditdata_, keydata_;
    CreditSystem credit_system_(msgdata_, creditdata_);

    credit_system_.SetMiningParameters(data_message_handler->calendar_handler->number_of_megabytes_for_mining,
                                       initial_difficulty, initial_diurnal_difficulty);
    StoreDataFromInitialDataMessageInCreditSystem(initial_data_message, credit_system_);

    CreditMessageHandler credit_message_handler(msgdata_, creditdata_, keydata_);
    credit_message_handler.SetCreditSystem(&credit_system_);

    BitChain spent_chain = initial_data_message.spent_chain;
    credit_message_handler.SetSpentChain(spent_chain);

    Calendar validation_calendar = credit_system->GetRequestedCalendar(initial_data_message);
    TrimLastDiurnFromCalendar(validation_calendar, &credit_system_);
    credit_message_handler.SetCalendar(validation_calendar);

    for (auto mined_credit_message : initial_data_message.mined_credit_messages_in_current_diurn)
        credit_message_handler.Handle(mined_credit_message, NULL);

    return TipOfCalendarMatchesTipOfInitialDataMessage(validation_calendar, initial_data_message);
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
