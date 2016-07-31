#include "gmock/gmock.h"

#include "TestPeer.h"
#include "DataMessageHandler.h"
#include "CalendarFailureMessage.h"
#include "CreditMessageHandler.h"

using namespace ::testing;
using namespace std;


#include "log.h"
#define LOG_CATEGORY "test"


class ADataMessageHandler : public Test
{
public:
    TestPeer peer;
    MemoryDataStore creditdata, msgdata, keydata;
    DataMessageHandler *data_message_handler;
    CreditMessageHandler *credit_message_handler;
    CreditSystem *credit_system;
    Calendar *calendar;

    virtual void SetUp()
    {
        data_message_handler = new DataMessageHandler(msgdata, creditdata);
        data_message_handler->SetNetwork(peer.network);
        credit_system = new CreditSystem(msgdata, creditdata);
        data_message_handler->SetCreditSystem(credit_system);
        calendar = new Calendar();
        data_message_handler->SetCalendar(calendar);

        credit_message_handler = new CreditMessageHandler(msgdata, creditdata, keydata);

        credit_message_handler->SetCreditSystem(credit_system);
        credit_message_handler->SetCalendar(*calendar);
        credit_message_handler->SetNetwork(peer.network);

        data_message_handler->calendar_scrutiny_time = 1500000;

        SetMiningPreferences();

        AddABatch();
    }

    void SetMiningPreferences()
    {
        credit_system->SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(1);
        credit_system->initial_difficulty = 100;
        credit_system->initial_diurnal_difficulty = 500;
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
        MinedCreditMessage msg = credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
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
    uint160 tip_request_hash = data_message_handler->RequestTips();
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
        tip.mined_credit = GetMinedCreditWithSpecifiedTotalWork(total_work);
        return tip;
    }

    MinedCredit GetMinedCreditWithSpecifiedTotalWork(uint160 total_work)
    {
        MinedCredit credit;
        credit.network_state.difficulty = 100;
        credit.network_state.previous_total_work = total_work - 100;
        credit.network_state.batch_number = 1;
        return credit;
    }

    virtual void SetUp()
    {
        ADataMessageHandler::SetUp();

        uint160 tip_request_hash = data_message_handler->RequestTips();
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
    data_message_handler->RequestCalendarOfTipWithTheMostWork();
    CalendarRequestMessage calendar_request(tip2.mined_credit);
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
    MinedCredit mined_credit_at_tip = calendar->LastMinedCredit();
    CalendarRequestMessage calendar_request(mined_credit_at_tip);
    data_message_handler->HandleMessage(GetDataStream(calendar_request), &peer);
    CalendarMessage calendar_message(calendar_request, credit_system);
    ASSERT_TRUE(peer.HasReceived("data", "calendar", calendar_message));
}

TEST_F(ADataMessageHandlerWithSomeBatches, RejectsIncomingCalendarMessagesThatWerentRequested)
{
    MinedCredit mined_credit_at_tip = calendar->LastMinedCredit();
    CalendarRequestMessage calendar_request(mined_credit_at_tip);
    CalendarMessage calendar_message(calendar_request, credit_system);

    data_message_handler->HandleMessage(GetDataStream(calendar_message), &peer);
    bool rejected = msgdata[calendar_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

CalendarMessage ADataMessageHandlerWithSomeBatches::ACalendarMessageThatWasRequested()
{
    CalendarRequestMessage calendar_request(calendar->LastMinedCredit());
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
    bool ok = data_message_handler->CheckDifficultiesRootsAndProofsOfWork(*calendar);
    ASSERT_THAT(ok, Eq(true));
}

class ADataMessageHandlerWithACalendarWithCalends : public ADataMessageHandlerWithSomeBatches
{
public:
    virtual void SetUp()
    {
        ADataMessageHandlerWithSomeBatches::SetUp();
        for (uint32_t i = 0; i < 20 or calendar->calends.size() < 2; i++)
        {
            AddABatch();
        }

        uint160 credit_hash = calendar->LastMinedCreditHash();
        vector<uint160> credit_hashes;
        while (credit_hash != 0)
        {
            credit_hashes.push_back(credit_hash);
            credit_hash = credit_system->PreviousCreditHash(credit_hash);
        }
        reverse(credit_hashes.begin(), credit_hashes.end());
    }

    virtual void TearDown()
    {
        ADataMessageHandlerWithSomeBatches::TearDown();
    }

    CalendarMessage CalendarMessageWithABadCalendar();

    CalendarMessage CalendarMessageWithACalendarWithTheMostWork();


    void AddABatchToCalendar(Calendar *calendar_)
    {
        credit_message_handler->calendar = calendar_;
        MinedCreditMessage msg = credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
        CompleteProofOfWork(msg);
        credit_system->StoreMinedCreditMessage(msg);
        calendar_->AddToTip(msg, credit_system);
        credit_message_handler->calendar = calendar;
    }

    InitialDataMessage AValidInitialDataMessageThatWasRequested();
};

TEST_F(ADataMessageHandlerWithACalendarWithCalends, ScrutinizesTheWorkInTheCalendar)
{
    CalendarFailureDetails details;
    uint64_t scrutiny_time = 1 * 1000000;
    bool ok = data_message_handler->Scrutinize(*calendar, scrutiny_time, details);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, ReturnsDetailsWhenACalendarFailsScrutiny)
{
    CalendarFailureDetails details;
    calendar->calends[1].proof_of_work.proof.link_lengths[2] += 1;
    uint64_t scrutiny_time = 1 * 1000000;
    while (data_message_handler->Scrutinize(*calendar, scrutiny_time, details)) {}
    ASSERT_THAT(details.credit_hash, Eq(calendar->calends[1].mined_credit.GetHash160()));
    ASSERT_THAT(details.check.failure_link, Eq(2));
}

CalendarMessage ADataMessageHandlerWithACalendarWithCalends::CalendarMessageWithABadCalendar()
{
    CalendarRequestMessage calendar_request(calendar->LastMinedCredit());
    msgdata[calendar_request.GetHash160()]["is_calendar_request"] = true;
    CalendarMessage calendar_message(calendar_request, credit_system);

    calendar_message.calendar.calends[1].proof_of_work.proof.link_lengths[2] += 1;
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

CalendarMessage ADataMessageHandlerWithACalendarWithCalends::CalendarMessageWithACalendarWithTheMostWork()
{
    Calendar new_calendar = *calendar;
    AddABatchToCalendar(&new_calendar);
    CalendarRequestMessage calendar_request(new_calendar.LastMinedCredit());
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
    bool spent_chain_ok = data_message_handler->CheckSpentChainInInitialDataMessage(initial_data_message);
    ASSERT_THAT(spent_chain_ok, Eq(false));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, AcceptsAGoodSpentChainIncludedInAnInitialDataMessage)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    bool spent_chain_ok = data_message_handler->CheckSpentChainInInitialDataMessage(initial_data_message);
    ASSERT_THAT(spent_chain_ok, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheInitialDataMessageDoesntContainAllTheEnclosedMessages)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    initial_data_message.enclosed_message_contents.pop_back();
    initial_data_message.enclosed_message_types.pop_back();
    bool data_contained = data_message_handler->EnclosedMessagesArePresentInInitialDataMessage(initial_data_message);
    ASSERT_THAT(data_contained, Eq(false));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheInitialDataMessageDoesContainAllTheEnclosedMessages)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    bool data_contained = data_message_handler->EnclosedMessagesArePresentInInitialDataMessage(initial_data_message);
    ASSERT_THAT(data_contained, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheInitialDataMessageMatchesTheRequestedCalendar)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    bool data_matches_calendar = data_message_handler->InitialDataMessageMatchesRequestedCalendar(initial_data_message);
    ASSERT_THAT(data_matches_calendar, Eq(true));
}

TEST_F(ADataMessageHandlerWithACalendarWithCalends, DetectsIfTheInitialDataMessageDoesntMatchTheRequestedCalendar)
{
    auto initial_data_message = AValidInitialDataMessageThatWasRequested();
    initial_data_message.mined_credit_messages_in_current_diurn.pop_back();
    bool data_matches_calendar = data_message_handler->InitialDataMessageMatchesRequestedCalendar(initial_data_message);
    ASSERT_THAT(data_matches_calendar, Eq(false));
}