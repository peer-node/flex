#include "gmock/gmock.h"

#include "TestPeer.h"
#include "DataMessageHandler.h"
#include "TipMessage.h"
#include "CalendarRequestMessage.h"
#include "CalendarMessage.h"

using namespace ::testing;
using namespace std;



class ADataMessageHandler : public Test
{
public:
    TestPeer peer;
    MemoryDataStore creditdata, msgdata;
    DataMessageHandler *data_message_handler;
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

        AddABatch();
    }

    virtual void TearDown()
    {
        delete data_message_handler;
        delete credit_system;
        delete calendar;
    }

    void AddABatch()
    {
        MinedCreditMessage msg;
        MinedCredit old_tip = calendar->LastMinedCredit();
        msg.mined_credit.network_state = credit_system->SucceedingNetworkState(old_tip);
        credit_system->StoreMinedCreditMessage(msg);
        calendar->AddToTip(msg, credit_system);
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


class ADataMessageHandlerWithSomeBatches : public ADataMessageHandler
{
public:
    virtual void SetUp()
    {
        ADataMessageHandler::SetUp();

        AddABatch();
        AddABatch();
    }

    virtual void TearDown()
    {
        ADataMessageHandler::TearDown();
    }
};

TEST_F(ADataMessageHandlerWithSomeBatches, RespondsToCalendarRequestsWithCalendarMessages)
{
    MinedCredit mined_credit_at_tip = calendar->LastMinedCredit();
    CalendarRequestMessage calendar_request(mined_credit_at_tip);
    data_message_handler->HandleMessage(GetDataStream(calendar_request), &peer);
    CalendarMessage calendar_message(calendar_request, credit_system);
    ASSERT_TRUE(peer.HasReceived("data", "calendar", calendar_message));
}
