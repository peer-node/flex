#ifndef FLEX_DATAREQUESTMESSAGEHANDLER_H
#define FLEX_DATAREQUESTMESSAGEHANDLER_H


#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include "MessageHandlerWithOrphanage.h"
#include "TipRequestMessage.h"
#include "CreditSystem.h"
#include "Calendar.h"
#include "TipMessage.h"
#include "CalendarRequestMessage.h"
#include "CalendarMessage.h"
#include "CalendarFailureDetails.h"
#include "InitialDataRequestMessage.h"
#include "InitialDataMessage.h"


#define CALENDAR_SCRUTINY_TIME 5000000 // microseconds


class DataMessageHandler : public MessageHandlerWithOrphanage
{
public:
    MemoryDataStore &msgdata, &creditdata;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    uint64_t calendar_scrutiny_time{CALENDAR_SCRUTINY_TIME};

    DataMessageHandler(MemoryDataStore &msgdata_, MemoryDataStore &creditdata_):
            MessageHandlerWithOrphanage(msgdata_), msgdata(msgdata_), creditdata(creditdata_)
    {
        channel = "data";
    }

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(TipRequestMessage);
        HANDLESTREAM(TipMessage);
        HANDLESTREAM(CalendarRequestMessage);
        HANDLESTREAM(CalendarMessage);
        HANDLESTREAM(InitialDataRequestMessage);
        HANDLESTREAM(InitialDataMessage);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(TipRequestMessage);
        HANDLEHASH(TipMessage);
        HANDLEHASH(CalendarRequestMessage);
        HANDLEHASH(CalendarMessage);
        HANDLEHASH(InitialDataRequestMessage);
        HANDLEHASH(InitialDataMessage);
    }

    HANDLECLASS(TipRequestMessage);
    HANDLECLASS(TipMessage);
    HANDLECLASS(CalendarRequestMessage);
    HANDLECLASS(CalendarMessage);
    HANDLECLASS(InitialDataRequestMessage);
    HANDLECLASS(InitialDataMessage);

    void HandleTipRequestMessage(TipRequestMessage request);

    void HandleTipMessage(TipMessage message);

    void HandleCalendarRequestMessage(CalendarRequestMessage request);

    void HandleInitialDataRequestMessage(InitialDataRequestMessage request);

    void HandleInitialDataMessage(InitialDataMessage request);

    void SetCreditSystem(CreditSystem *credit_system_);

    void SetCalendar(Calendar *calendar_);

    uint160 RequestTips();

    uint160 RequestCalendarOfTipWithTheMostWork();

    TipMessage TipWithTheMostWork();

    uint160 SendCalendarRequestToPeer(CNode *peer, MinedCredit mined_credit);

    std::vector<uint160> TipMessageHashesWithTheMostWork();

    void RecordReportedTotalWorkOfTip(uint160 tip_message_hash, uint160 total_work);

    void HandleCalendarMessage(CalendarMessage calendar_message);

    bool ScrutinizeCalendarWithTheMostWork();

    bool CheckDifficultiesRootsAndProofsOfWork(Calendar &calendar);

    bool Scrutinize(Calendar calendar, uint64_t scrutiny_time, CalendarFailureDetails &details);

    void HandleIncomingCalendar(CalendarMessage incoming_calendar);

    bool ValidateCalendarMessage(CalendarMessage calendar_message);

    void SwitchToCalendarWithTheMostWorkWhichHasSurvivedScrutiny();

    Calendar CalendarWithTheMostWorkWhichHasSurvivedScrutiny();

    void SwitchToCalendar(Calendar new_calendar);

    void RequestInitialDataMessage(CalendarMessage calendar_message);

    bool CheckSpentChainInInitialDataMessage(InitialDataMessage message);

    bool EnclosedMessagesArePresentInInitialDataMessage(InitialDataMessage &message);

    MemoryDataStore GetEnclosedMessageHashes(InitialDataMessage &message);

    bool InitialDataMessageMatchesRequestedCalendar(InitialDataMessage &initial_data_message);

    Calendar GetRequestedCalendar(InitialDataMessage &initial_data_message);

    bool InitialDataMessageMatchesCalendar(InitialDataMessage &message, Calendar calendar);
};


#endif //FLEX_DATAREQUESTMESSAGEHANDLER_H
