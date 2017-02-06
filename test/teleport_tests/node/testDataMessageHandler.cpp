#include "gmock/gmock.h"

#include "TestPeer.h"
#include "data_handler/DataMessageHandler.h"
#include "test/teleport_tests/node/credit_handler/CreditMessageHandler.h"
#include "TeleportNetworkNode.h"
#include "data_handler/TipHandler.h"
#include "data_handler/CalendarHandler.h"
#include "data_handler/InitialDataHandler.h"
#include "data_handler/KnownHistoryHandler.h"

using namespace ::testing;
using namespace std;


#include "log.h"
#define LOG_CATEGORY "test"


class ADataMessageHandler : public Test
{
public:
    TestPeer peer;
    MemoryDataStore creditdata, msgdata, keydata;
    Data *data;
    DataMessageHandler *data_message_handler;
    CreditMessageHandler *credit_message_handler;
    CreditSystem *credit_system;
    Calendar *calendar;

    virtual void SetUp()
    {
        peer.id = 1;

        data = new Data(msgdata, creditdata, keydata);
        data_message_handler = new DataMessageHandler(msgdata, creditdata);
        data_message_handler->SetNetwork(peer.network);
        credit_system = new CreditSystem(msgdata, creditdata);
        data_message_handler->SetCreditSystemAndGenerateHandlers(credit_system);
        calendar = new Calendar();
        data_message_handler->SetCalendar(calendar);

        credit_message_handler = new CreditMessageHandler(*data);

        credit_message_handler->SetCreditSystem(credit_system);
        credit_message_handler->SetCalendar(*calendar);
        credit_message_handler->SetNetwork(peer.network);

        data_message_handler->calendar_handler->calendar_scrutiny_time = 1500000;

        SetMiningPreferences();

        AddABatch();
    }

    void SetMiningPreferences()
    {
        credit_system->SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(1);
        credit_system->initial_difficulty = 100;
        credit_system->initial_diurnal_difficulty = 500;
        data_message_handler->initial_data_handler->SetMiningParametersForInitialDataMessageValidation(1, 100, 500);
    }

    virtual void TearDown()
    {
        delete credit_message_handler;
        delete data_message_handler;
        delete credit_system;
        delete calendar;
    }

    void AddABatch()
    {
        MinedCreditMessage msg = credit_message_handler->builder->GenerateMinedCreditMessageWithoutProofOfWork();
        CompleteProofOfWork(msg);
        credit_system->StoreMinedCreditMessage(msg);
        calendar->AddToTip(msg, credit_system);
    }

    void CompleteProofOfWork(MinedCreditMessage& msg)
    {
        credit_system->AddIncompleteProofOfWork(msg);
        uint8_t keep_working = 1;
        msg.proof_of_work.proof.DoWork(&keep_working);
    }

    template <typename T>
    CDataStream GetDataStream(T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << std::string("data") << message.Type() << message;
        return ss;
    }
};

TEST_F(ADataMessageHandler, SendsTheTipWhenRequested)
{
    TipRequestMessage tip_request_message;
    data_message_handler->HandleMessage(GetDataStream(tip_request_message), &peer);
    TipMessage tip_message(tip_request_message, calendar);
    ASSERT_TRUE(peer.HasReceived("data", "tip", tip_message));
}

TEST_F(ADataMessageHandler, RejectsIncomingTipMessagesWhichWerentRequested)
{
    TipRequestMessage tip_request_message;
    TipMessage tip_message(tip_request_message, calendar);
    data_message_handler->HandleMessage(GetDataStream(tip_message), &peer);
    bool rejected = msgdata[tip_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ADataMessageHandler, AcceptsIncomingTipMessagesWhichWereRequested)
{
    uint160 tip_request_hash = data_message_handler->tip_handler->RequestTips();
    TipRequestMessage tip_request_message = msgdata[tip_request_hash]["tip_request"];
    TipMessage tip_message(tip_request_message, calendar);
    data_message_handler->HandleMessage(GetDataStream(tip_message), &peer);
    bool rejected = msgdata[tip_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(false));
}


class ADataMessageHandlerWhichReceivedMultipleTips : public ADataMessageHandler
{
public:
    TipMessage tip1, tip2;

    TipMessage GetTipMessage(TipRequestMessage request, uint160 total_work)
    {
        TipMessage tip;
        tip.tip_request_hash = request.GetHash160();
        tip.mined_credit_message = GetMinedCreditMessageWithSpecifiedTotalWork(total_work);
        return tip;
    }

    MinedCreditMessage GetMinedCreditMessageWithSpecifiedTotalWork(uint160 total_work)
    {
        MinedCredit credit;
        credit.network_state.difficulty = 100;
        credit.network_state.previous_total_work = total_work - 100;
        credit.network_state.batch_number = 1;

        MinedCreditMessage msg;
        msg.mined_credit = credit;
        return msg;
    }

    virtual void SetUp()
    {
        ADataMessageHandler::SetUp();

        uint160 tip_request_hash = data_message_handler->tip_handler->RequestTips();
        TipRequestMessage request = msgdata[tip_request_hash]["tip_request"];

        tip1 = GetTipMessage(request, 1000);
        data_message_handler->HandleMessage(GetDataStream(tip1), &peer);
        tip2 = GetTipMessage(request, 2000);
        data_message_handler->HandleMessage(GetDataStream(tip2), &peer);
    }

    virtual void TearDown()
    {
        ADataMessageHandler::TearDown();
    }
};

TEST_F(ADataMessageHandlerWhichReceivedMultipleTips, RequestsTheCalendarOfTheTipWithTheMostReportedWork)
{
    data_message_handler->tip_handler->RequestCalendarOfTipWithTheMostWork();
    CalendarRequestMessage calendar_request(tip2.mined_credit_message);
    ASSERT_TRUE(peer.HasReceived("data", "calendar_request", calendar_request));
}


class ADataMessageHandlerWithSomeBatches : public ADataMessageHandlerWhichReceivedMultipleTips
{
public:
    virtual void SetUp()
    {
        ADataMessageHandlerWhichReceivedMultipleTips::SetUp();

        AddABatch();
        AddABatch();
    }

    virtual void TearDown()
    {
        ADataMessageHandlerWhichReceivedMultipleTips::TearDown();
    }

    CalendarMessage ACalendarMessageThatWasRequested();
};

TEST_F(ADataMessageHandlerWithSomeBatches, RespondsToCalendarRequestsWithCalendarMessages)
{
    CalendarRequestMessage calendar_request(calendar->LastMinedCreditMessage());
    data_message_handler->HandleMessage(GetDataStream(calendar_request), &peer);
    CalendarMessage calendar_message(calendar_request, credit_system);
    ASSERT_TRUE(peer.HasReceived("data", "calendar", calendar_message));
}

TEST_F(ADataMessageHandlerWithSomeBatches, RejectsIncomingCalendarMessagesThatWerentRequested)
{
    CalendarRequestMessage calendar_request(calendar->LastMinedCreditMessage());
    CalendarMessage calendar_message(calendar_request, credit_system);

    data_message_handler->HandleMessage(GetDataStream(calendar_message), &peer);
    bool rejected = msgdata[calendar_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

CalendarMessage ADataMessageHandlerWithSomeBatches::ACalendarMessageThatWasRequested()
{
    CalendarRequestMessage calendar_request(calendar->LastMinedCreditMessage());
    msgdata[calendar_request.GetHash160()]["is_calendar_request"] = true;
    return CalendarMessage(calendar_request, credit_system);
}

TEST_F(ADataMessageHandlerWithSomeBatches, AcceptsIncomingCalendarMessagesThatWereRequested)
{
    auto calendar_message = ACalendarMessageThatWasRequested();
    data_message_handler->HandleMessage(GetDataStream(calendar_message), &peer);
    bool rejected = msgdata[calendar_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(false));
}

TEST_F(ADataMessageHandlerWithSomeBatches, ChecksTheDifficultiesRootsAndProofsOfWorkInACalendar)
{
    bool ok = data_message_handler->calendar_handler->CheckDifficultiesRootsAndProofsOfWork(*calendar);
    ASSERT_THAT(ok, Eq(true));
}

class ADataMessageHandlerWithACalendarWithCalends : public ADataMessageHandlerWithSomeBatches
{
public:
    virtual void SetUp()
    {
        ADataMessageHandlerWithSomeBatches::SetUp();

        static Calendar reference_test_calendar;
        static MemoryDataStore reference_test_msgdata;
        static MemoryDataStore reference_test_creditdata;
        
        if (reference_test_calendar.calends.size() == 0)
        {
            for (uint32_t i = 0; i < 20 or calendar->calends.size() < 2; i++)
            {
                AddABatch();
            }
            reference_test_calendar = *calendar;
            reference_test_msgdata = msgdata;
            reference_test_creditdata = creditdata;
        }
        else
        {
            *calendar = reference_test_calendar;
            msgdata = reference_test_msgdata;
            creditdata = reference_test_creditdata;
        }
    }

    virtual void TearDown()
    {
        ADataMessageHandlerWithSomeBatches::TearDown();
    }

    void AddABatchToCalendar(Calendar *calendar_)
    {
        credit_message_handler->builder->calendar = calendar_;
        MinedCreditMessage msg = credit_message_handler->builder->GenerateMinedCreditMessageWithoutProofOfWork();
        CompleteProofOfWork(msg);
        credit_system->StoreMinedCreditMessage(msg);
        calendar_->AddToTip(msg, credit_system);
        credit_message_handler->builder->calendar = calendar;
    }

    CalendarMessage CalendarMessageWithABadCalendar();

    CalendarMessage CalendarMessageWithACalendarWithTheMostWork();

    InitialDataMessage AValidInitialDataMessageThatWasRequested();
};

TEST_F(ADataMessageHandlerWithACalendarWithCalends, ScrutinizesTheWorkInTheCalendar)
{
    CalendarFailureDetails details;
    uint64_t scrutiny_time = 1 * 1000000;
    bool ok = data_message_handler->calendar_handler->Scrutinize(*calendar, scrutiny_time, details);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, ReturnsDetailsWhenACalendarFailsScrutiny)
{
    CalendarFailureDetails details;
    calendar->calends[1].proof_of_work.proof.link_lengths[2] += 1;
    uint64_t scrutiny_time = 1 * 1000000;
    while (data_message_handler->calendar_handler->Scrutinize(*calendar, scrutiny_time, details)) {}
    ASSERT_THAT(details.mined_credit_message_hash, Eq(calendar->calends[1].GetHash160()));
    ASSERT_THAT(details.check.failure_link, Eq(2));
}

CalendarMessage ADataMessageHandlerWithACalendarWithCalends::CalendarMessageWithABadCalendar()
{
    CalendarRequestMessage calendar_request(calendar->LastMinedCreditMessage());
    msgdata[calendar_request.GetHash160()]["is_calendar_request"] = true;
    CalendarMessage calendar_message(calendar_request, credit_system);

    // introduce a failure into the proof of work of the second calend, then
    // truncate the calendar after the bad calend and add more batches after it

    calendar_message.calendar.calends[1].proof_of_work.proof.link_lengths[2] += 1;
    credit_system->StoreMinedCreditMessage(calendar_message.calendar.calends[1]);
    calendar_message.calendar = Calendar(calendar_message.calendar.calends[1].GetHash160(), credit_system);

    for (uint32_t i = 0; i < 20; i++)
        AddABatchToCalendar(&calendar_message.calendar);

    return calendar_message;
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, SendsACalendarFailureMessageWithFailureDetails)
{
    auto calendar_message = CalendarMessageWithABadCalendar();

    CalendarFailureDetails details;
    while (details.check.failure_link == 0)
    {
        data_message_handler->HandleMessage(GetDataStream(calendar_message), &peer);
        details = creditdata[calendar_message.GetHash160()]["failure_details"];
    }
    ASSERT_THAT(details.check.failure_link, Eq(2));
    ASSERT_TRUE(peer.HasBeenInformedAbout("data", "calendar_failure", CalendarFailureMessage(details)));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends,
       RecordsTheMinedCreditMessageHashIfACalendarFailureMessageRefersToACalendNotYetReceived)
{
    CalendarFailureDetails details;
    details.mined_credit_message_hash = 2;

    CalendarFailureMessage failure_message(details);
    data_message_handler->HandleMessage(GetDataStream(failure_message), &peer);

    bool calend_has_reported_failure = credit_system->CalendHasReportedFailure(details.mined_credit_message_hash);
    ASSERT_THAT(calend_has_reported_failure, Eq(true));
}



CalendarMessage ADataMessageHandlerWithACalendarWithCalends::CalendarMessageWithACalendarWithTheMostWork()
{
    Calendar new_calendar = *calendar;
    AddABatchToCalendar(&new_calendar);
    CalendarRequestMessage calendar_request(new_calendar.LastMinedCreditMessage());
    msgdata[calendar_request.GetHash160()]["is_calendar_request"] = true;
    CalendarMessage calendar_message(calendar_request, credit_system);
    return calendar_message;
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, RequestsInitialDataForTheCalendarWithTheMostWork)
{
    auto calendar_message = CalendarMessageWithACalendarWithTheMostWork();
    data_message_handler->HandleMessage(GetDataStream(calendar_message), &peer);
    InitialDataRequestMessage initial_data_request(calendar_message);
    ASSERT_TRUE(peer.HasReceived("data", "initial_data_request", initial_data_request));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, RespondsToInitialDataRequestsWithInitialDataMessages)
{
    auto calendar_message = ACalendarMessageThatWasRequested();
    InitialDataRequestMessage initial_data_request(calendar_message);
    data_message_handler->HandleMessage(GetDataStream(initial_data_request), &peer);
    InitialDataMessage initial_data_message(initial_data_request, credit_system);
    ASSERT_TRUE(peer.HasReceived("data", "initial_data", initial_data_message));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, RejectsInitialDataMessagesWhichWerentRequested)
{
    auto calendar_message = ACalendarMessageThatWasRequested();
    InitialDataMessage initial_data_message(InitialDataRequestMessage(calendar_message), credit_system);
    data_message_handler->HandleMessage(GetDataStream(initial_data_message), &peer);
    bool rejected = msgdata[initial_data_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

InitialDataMessage ADataMessageHandlerWithACalendarWithCalends::AValidInitialDataMessageThatWasRequested()
{
    auto calendar_message = ACalendarMessageThatWasRequested();
    InitialDataRequestMessage data_request(calendar_message);
    uint160 request_hash = data_request.GetHash160();
    uint160 calendar_message_hash = calendar_message.GetHash160();
    msgdata[request_hash]["is_data_request"] = true;
    msgdata[request_hash]["calendar_message_hash"] = calendar_message_hash;
    msgdata[calendar_message_hash]["calendar"] = calendar_message;

    return InitialDataMessage(data_request, credit_system);
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, AcceptsValidInitialDataMessagesWhichWereRequested)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    data_message_handler->HandleMessage(GetDataStream(initial_data_message), &peer);
    bool rejected = msgdata[initial_data_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(false));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsABadSpentChainIncludedInAnInitialDataMessage)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    initial_data_message.spent_chain.Set(0);
    bool spent_chain_ok = data_message_handler->initial_data_handler->CheckSpentChainInInitialDataMessage(initial_data_message);
    ASSERT_THAT(spent_chain_ok, Eq(false));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, AcceptsAGoodSpentChainIncludedInAnInitialDataMessage)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    bool spent_chain_ok = data_message_handler->initial_data_handler->CheckSpentChainInInitialDataMessage(initial_data_message);
    ASSERT_THAT(spent_chain_ok, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheInitialDataMessageDoesntContainAllTheEnclosedMessages)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    initial_data_message.message_data.enclosed_message_contents.pop_back();
    initial_data_message.message_data.enclosed_message_types.pop_back();
    bool data_contained = data_message_handler->initial_data_handler->EnclosedMessagesArePresentInInitialDataMessage(initial_data_message);
    ASSERT_THAT(data_contained, Eq(false));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheInitialDataMessageDoesContainAllTheEnclosedMessages)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    bool data_contained = data_message_handler->initial_data_handler->EnclosedMessagesArePresentInInitialDataMessage(initial_data_message);
    ASSERT_THAT(data_contained, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheInitialDataMessageMatchesTheRequestedCalendar)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    bool data_matches_calendar = data_message_handler->initial_data_handler->InitialDataMessageMatchesRequestedCalendar(initial_data_message);
    ASSERT_THAT(data_matches_calendar, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheInitialDataMessageDoesntMatchTheRequestedCalendar)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    initial_data_message.mined_credit_messages_in_current_diurn.pop_back();
    bool data_matches_calendar = data_message_handler->initial_data_handler->InitialDataMessageMatchesRequestedCalendar(initial_data_message);
    ASSERT_THAT(data_matches_calendar, Eq(false));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheMinedCreditMessagesInTheInitialDataMessageAreValid)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    bool msgs_ok = data_message_handler->initial_data_handler->ValidateMinedCreditMessagesInInitialDataMessage(initial_data_message);
    ASSERT_THAT(msgs_ok, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheMinedCreditMessagesInTheInitialDataMessageAreInvalid)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    initial_data_message.mined_credit_messages_in_current_diurn[0].hash_list.short_hashes.pop_back();
    bool msgs_ok = data_message_handler->initial_data_handler->ValidateMinedCreditMessagesInInitialDataMessage(initial_data_message);
    ASSERT_THAT(msgs_ok, Eq(false));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, MarksTheMinedCreditMessagesInAValidInitialDataMessageAsValidated)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    uint160 last_msg_hash = initial_data_message.mined_credit_messages_in_current_diurn.back().GetHash160();

    bool valid = creditdata[last_msg_hash]["passed_verification"];
    ASSERT_THAT(valid, Eq(false));

    data_message_handler->initial_data_handler->MarkMinedCreditMessagesInInitialDataMessageAsValidated(initial_data_message);

    valid = creditdata[last_msg_hash]["passed_verification"];
    ASSERT_THAT(valid, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, ProvidesAKnownHistoryMessage)
{
    auto known_history_message = data_message_handler->known_history_handler->GenerateKnownHistoryMessage();
    KnownHistoryDeclaration history_declaration = known_history_message.history_declaration;

    ASSERT_THAT(history_declaration.known_diurns.Get(0), Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, SpecifiesDiurnHashesForKnownDiurnsInTheKnownHistoryMessage)
{
    auto known_history_message = data_message_handler->known_history_handler->GenerateKnownHistoryMessage();
    KnownHistoryDeclaration history_declaration = known_history_message.history_declaration;

    ASSERT_THAT(history_declaration.diurn_hashes.size(), Eq(calendar->calends.size()));

    uint160 first_diurn_root = calendar->calends[0].DiurnRoot();
    uint160 stored_diurn_hash = credit_system->creditdata[first_diurn_root]["diurn_hash"];
    ASSERT_THAT(history_declaration.diurn_hashes[0], Eq(stored_diurn_hash));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, SpecifiesDiurnMessageDataHashesForKnownDiurnsInTheKnownHistoryMessage)
{
    auto known_history_message = data_message_handler->known_history_handler->GenerateKnownHistoryMessage();
    KnownHistoryDeclaration history_declaration = known_history_message.history_declaration;

    ASSERT_THAT(history_declaration.diurn_hashes.size(), Eq(calendar->calends.size()));

    uint160 first_diurn_root = calendar->calends[0].DiurnRoot();
    Calend first_calend = calendar->calends[0];
    DiurnMessageData message_data = first_calend.GenerateDiurnMessageData(credit_system);

    uint160 message_data_hash = message_data.GetHash160();

    ASSERT_THAT(history_declaration.diurn_message_data_hashes[0], Eq(message_data_hash));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsWhenAKnownHistoryMessageIsValid)
{
    auto known_history_message = data_message_handler->known_history_handler->GenerateKnownHistoryMessage();
    bool ok = data_message_handler->known_history_handler->ValidateKnownHistoryMessage(known_history_message);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsWhenAKnownHistoryMessageHasASizeMismatch)
{
    auto known_history_message = data_message_handler->known_history_handler->GenerateKnownHistoryMessage();
    known_history_message.history_declaration.known_diurns.Add();
    bool ok = data_message_handler->known_history_handler->ValidateKnownHistoryMessage(known_history_message);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsWhenAKnownHistoryMessageDoesntMatchTheCalendar)
{
    auto known_history_message = data_message_handler->known_history_handler->GenerateKnownHistoryMessage();
    known_history_message.mined_credit_message_hash += 1;
    bool ok = data_message_handler->known_history_handler->ValidateKnownHistoryMessage(known_history_message);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, RejectsAKnownHistoryMessageWhichWasntRequested)
{
    auto known_history_message = data_message_handler->known_history_handler->GenerateKnownHistoryMessage();
    data_message_handler->HandleMessage(GetDataStream(known_history_message), &peer);
    bool rejected = msgdata[known_history_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, RecordsWhichPeersKnowWhichDiurnsWhenHandlingAKnownHistoryMessage)
{
    auto known_history_message = data_message_handler->known_history_handler->GenerateKnownHistoryMessage();
    msgdata[known_history_message.request_hash]["is_history_request"] = true;
    data_message_handler->HandleMessage(GetDataStream(known_history_message), &peer);

    uint160 first_diurn_root = calendar->calends[0].DiurnRoot();
    std::vector<int> ids_of_peers_who_know_the_first_diurn = msgdata[first_diurn_root]["peers_with_data"];
    ASSERT_TRUE(VectorContainsEntry(ids_of_peers_who_know_the_first_diurn, peer.id));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, SendsAKnownHistoryMessageWhenRequested)
{
    KnownHistoryRequest history_request(calendar->LastMinedCreditMessageHash());
    data_message_handler->HandleMessage(GetDataStream(history_request), &peer);
    KnownHistoryMessage history_message(history_request, credit_system);
    ASSERT_TRUE(peer.HasReceived("data", "known_history", history_message));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, RequestsKnownHistoryMessages)
{
    uint160 msg_hash = calendar->LastMinedCreditMessageHash();
    uint160 request_hash = data_message_handler->known_history_handler->RequestKnownHistoryMessages(msg_hash);
    KnownHistoryRequest request = msgdata[request_hash]["known_history_request"];
    ASSERT_TRUE(peer.HasBeenInformedAbout("data", "known_history_request", request));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, RequestsDiurnData)
{
    uint160 msg_hash = calendar->LastMinedCreditMessageHash();
    std::vector<uint32_t> requested_diurns{1, 2};
    KnownHistoryMessage history_message(KnownHistoryRequest(msg_hash), credit_system);
    uint160 request_hash = data_message_handler->known_history_handler->RequestDiurnData(history_message,
                                                                                         requested_diurns, &peer);
    DiurnDataRequest request = msgdata[request_hash]["diurn_data_request"];
    ASSERT_TRUE(peer.HasReceived("data", "diurn_data_request", request));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, RespondsToDiurnDataRequestsWithDiurnDataMessages)
{
    uint160 msg_hash = calendar->LastMinedCreditMessageHash();
    DiurnDataRequest request(KnownHistoryMessage(KnownHistoryRequest(msg_hash), credit_system),
                             std::vector<uint32_t>{1, 2});
    data_message_handler->HandleMessage(GetDataStream(request), &peer);
    DiurnDataMessage diurn_data_message(request, credit_system);
    ASSERT_TRUE(peer.HasReceived("data", "diurn_data", diurn_data_message));
}

class ADataMessageHandlerWithADiurnDataMessage : public ADataMessageHandlerWithACalendarWithCalends
{
public:
    KnownHistoryRequest request;
    KnownHistoryMessage known_history;
    DiurnDataRequest diurn_data_request;
    DiurnDataMessage diurn_data_message;

    virtual void SetUp()
    {
        ADataMessageHandlerWithACalendarWithCalends::SetUp();

        uint160 msg_hash = calendar->LastMinedCreditMessageHash();
        request = KnownHistoryRequest(msg_hash);
        known_history = KnownHistoryMessage(request, credit_system);
        diurn_data_request = DiurnDataRequest(known_history, std::vector<uint32_t>{1, 2});
        diurn_data_message = DiurnDataMessage(diurn_data_request, credit_system);
    }

    virtual void TearDown()
    {
        ADataMessageHandlerWithACalendarWithCalends::TearDown();
    }
};

TEST_F(ADataMessageHandlerWithADiurnDataMessage, RejectsDiurnDataMessagesThatWerentRequested)
{
    data_message_handler->HandleMessage(GetDataStream(diurn_data_message), &peer);
    bool rejected = msgdata[diurn_data_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ADataMessageHandlerWithADiurnDataMessage, DetectsIfTheContentsOfTheDiurnDataMessageMatchTheKnownHistoryMessage)
{
    bool ok = data_message_handler->known_history_handler->CheckIfDiurnDataMatchesHashes(diurn_data_message,
                                                                                         known_history,
                                                                                         diurn_data_request);

    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ADataMessageHandlerWithADiurnDataMessage,
       DetectsIfTheContentsOfTheDiurnDataMessageDontMatchTheKnownHistoryMessage)
{
    known_history.history_declaration.diurn_hashes[1] += 1;

    bool ok = data_message_handler->known_history_handler->CheckIfDiurnDataMatchesHashes(diurn_data_message,
                                                                                         known_history,
                                                                                         diurn_data_request);

    ASSERT_THAT(ok, Eq(false));
}

TEST_F(ADataMessageHandlerWithADiurnDataMessage, DetectsIfTheDataInTheDiurnDataMessageIsValid)
{
    bool ok = data_message_handler->known_history_handler->ValidateDataInDiurnDataMessage(diurn_data_message);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ADataMessageHandlerWithADiurnDataMessage, DetectsIfTheNumbersOfEntriesInTheDiurnDataMessageDontMatch)
{
    auto original_data_message = diurn_data_message;
    auto bad_diurn_data_message = diurn_data_message;
    bad_diurn_data_message.initial_spent_chains.pop_back();
    bool ok = data_message_handler->known_history_handler->ValidateDataInDiurnDataMessage(bad_diurn_data_message);
    ASSERT_THAT(ok, Eq(false));

    bad_diurn_data_message = diurn_data_message;
    bad_diurn_data_message.diurns.pop_back();
    ok = data_message_handler->known_history_handler->ValidateDataInDiurnDataMessage(bad_diurn_data_message);
    ASSERT_THAT(ok, Eq(false));

    bad_diurn_data_message = diurn_data_message;
    bad_diurn_data_message.message_data.pop_back();
    ok = data_message_handler->known_history_handler->ValidateDataInDiurnDataMessage(bad_diurn_data_message);
    ASSERT_THAT(ok, Eq(false));
}


class ADataMessageHandlerWithADiurnMessageWithABadProofOfWork : public ADataMessageHandlerWithACalendarWithCalends
{
public:
    KnownHistoryRequest request;
    KnownHistoryMessage known_history;
    DiurnDataRequest diurn_data_request;
    DiurnDataMessage diurn_data_message;
    uint32_t bad_batch_number, bad_diurn_number;
    uint160 bad_mined_credit_message_hash;
    uint160 hash_of_calend_preceding_bad_batch, hash_of_calend_succeeding_bad_batch;

    virtual void SetUp()
    {
        ADataMessageHandlerWithSomeBatches::SetUp();

        BuildACalendarWithABadProofOfWorkInADiurn();

        uint160 msg_hash = calendar->LastMinedCreditMessageHash();
        request = KnownHistoryRequest(msg_hash);
        known_history = KnownHistoryMessage(request, credit_system);
        diurn_data_request = DiurnDataRequest(known_history, std::vector<uint32_t>{bad_diurn_number});
        msgdata[diurn_data_request.known_history_message_hash]["known_history"] = known_history;
        diurn_data_message = DiurnDataMessage(diurn_data_request, credit_system);
        msgdata[diurn_data_message.request_hash]["diurn_data_request"] = diurn_data_request;
        msgdata[diurn_data_message.request_hash]["is_diurn_data_request"] = true;
    }

    void AddTipToCalendar(MinedCreditMessage tip)
    {
        credit_system->AcceptMinedCreditMessageAsValidByRecordingTotalWorkAndParent(tip);
        credit_system->StoreMinedCreditMessage(tip);
        creditdata[tip.GetHash160()]["passed_verification"] = true;

        calendar->AddToTip(tip, credit_system);
    }

    void IntroduceAFailureIntoTheProofOfWorkAtTheTip()
    {
        auto tip = calendar->LastMinedCreditMessage();

        calendar->RemoveLast(credit_system);
        credit_system->RemoveFromMainChainAndDeleteRecordOfTotalWork(tip);

        tip.proof_of_work.proof.link_lengths[2] += 1;

        AddTipToCalendar(tip);

        bad_diurn_number = calendar->calends.size();
        bad_batch_number = tip.mined_credit.network_state.batch_number;
        bad_mined_credit_message_hash = tip.GetHash160();
    }

    void BuildACalendarWithABadProofOfWorkInADiurn()
    {
        for (uint32_t i = 0; calendar->calends.size() < 2; i++)
            AddABatch();

        for (uint32_t i = 0; calendar->current_diurn.Size() < 2; i++)
            AddABatch();

        hash_of_calend_preceding_bad_batch = calendar->current_diurn.credits_in_diurn[0].GetHash160();

        IntroduceAFailureIntoTheProofOfWorkAtTheTip();

        while (credit_system->IsCalend(calendar->LastMinedCreditMessageHash()))
            IntroduceAFailureIntoTheProofOfWorkAtTheTip();

        uint64_t number_of_calends_so_far = calendar->calends.size();

        while (calendar->calends.size() == number_of_calends_so_far)
            AddABatch();

        hash_of_calend_succeeding_bad_batch = calendar->current_diurn.credits_in_diurn[0].GetHash160();

        AddABatch();

        while (calendar->calends.size() < 5)
            AddABatch();
    }

    virtual void TearDown()
    {
        ADataMessageHandlerWithACalendarWithCalends::TearDown();
    }
};

TEST_F(ADataMessageHandlerWithADiurnMessageWithABadProofOfWork, FindsTheFailureInTheProofOfWork)
{
    uint64_t scrutiny_time = 60 * 1000000; // microseconds
    DiurnFailureDetails failure_details;
    KnownHistoryHandler *history_handler = data_message_handler->known_history_handler;
    bool ok = history_handler->CheckForFailuresInProofsOfWorkInDiurnDataMessage(diurn_data_message,
                                                                                failure_details,
                                                                                bad_diurn_number,
                                                                                scrutiny_time);
    ASSERT_THAT(ok, Eq(false));
    ASSERT_THAT(failure_details.mined_credit_message_hash, Eq(bad_mined_credit_message_hash));
    ASSERT_THAT(failure_details.preceding_calend_hash, Eq(hash_of_calend_preceding_bad_batch));
    ASSERT_THAT(failure_details.succeeding_calend_hash, Eq(hash_of_calend_succeeding_bad_batch));
    ASSERT_THAT(bad_diurn_number, Eq(0));
}

TEST_F(ADataMessageHandlerWithADiurnMessageWithABadProofOfWork, DetectsIfTheMessageAndCalendHashesMatch)
{
    bool ok = data_message_handler->known_history_handler->CheckHashesInDiurnDataMessage(diurn_data_message);
    ASSERT_THAT(ok, Eq(true));

    diurn_data_message.calends[0].mined_credit.network_state.previous_calend_hash += 1;
    ok = data_message_handler->known_history_handler->CheckHashesInDiurnDataMessage(diurn_data_message);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(ADataMessageHandlerWithADiurnMessageWithABadProofOfWork, SendsADiurnFailureMessage)
{
    KnownHistoryHandler *history_handler = data_message_handler->known_history_handler;
    history_handler->diurn_scrutiny_time = 60 * 1000000; // microseconds
    data_message_handler->HandleMessage(GetDataStream(diurn_data_message), &peer);
    uint160 diurn_failure_message_hash = msgdata[diurn_data_message.GetHash160()]["failure_message_hash"];
    DiurnFailureMessage failure_message;
    failure_message = msgdata[diurn_failure_message_hash]["diurn_failure"];
    ASSERT_TRUE(peer.HasBeenInformedAbout("data", "diurn_failure", failure_message));
}

class ADataMessageHandlerWithADiurnFailureMessage : public ADataMessageHandlerWithADiurnMessageWithABadProofOfWork
{
public:
    DiurnFailureMessage failure_message;

    virtual void SetUp()
    {
        ADataMessageHandlerWithADiurnMessageWithABadProofOfWork::SetUp();
        DiurnFailureDetails failure_details;
        KnownHistoryHandler *history_handler = data_message_handler->known_history_handler;
        bool ok = true;
        while (ok)
            ok = history_handler->CheckForFailuresInMinedCreditMessageFromDiurnDataMessage(diurn_data_message,
                                                                                           failure_details,
                                                                                           bad_diurn_number,
                                                                                           0, 1);
        failure_message.details = failure_details;
        failure_message.diurn = diurn_data_message.diurns[bad_diurn_number];
    }

    virtual void TearDown()
    {
        ADataMessageHandlerWithADiurnMessageWithABadProofOfWork::TearDown();
    }
};

TEST_F(ADataMessageHandlerWithADiurnFailureMessage, DetectsIfTheDiurnFailureMessageIsValid)
{
    bool ok = data_message_handler->known_history_handler->ValidateDiurnFailureMessage(failure_message);
    ASSERT_THAT(ok, Eq(true));
}


TEST_F(ADataMessageHandlerWithADiurnFailureMessage, DetectsIfTheDiurnFailureMessageIsInvalid)
{
    failure_message.diurn.credits_in_diurn.back().mined_credit.network_state.timestamp += 1;
    bool ok = data_message_handler->known_history_handler->ValidateDiurnFailureMessage(failure_message);
    ASSERT_THAT(ok, Eq(false));

    failure_message.diurn.credits_in_diurn.back().mined_credit.network_state.timestamp -= 1;
    failure_message.details.preceding_calend_hash += 1;
    ok = data_message_handler->known_history_handler->ValidateDiurnFailureMessage(failure_message);
    ASSERT_THAT(ok, Eq(false));

    failure_message.details.preceding_calend_hash -= 1;
    failure_message.details.mined_credit_message_hash += 1;
    ok = data_message_handler->known_history_handler->ValidateDiurnFailureMessage(failure_message);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(ADataMessageHandlerWithADiurnFailureMessage, MarksTheBadPartsOfTheCalendarAsInvalid)
{
    uint160 original_calendar_tip = calendar->LastMinedCreditMessageHash();
    data_message_handler->HandleMessage((GetDataStream(failure_message)), &peer);
    credit_system->SwitchMainChainToTipWithTheMostWork();
    ASSERT_THAT(credit_system->CurrentTipOfMainChain(), Ne(original_calendar_tip));
}