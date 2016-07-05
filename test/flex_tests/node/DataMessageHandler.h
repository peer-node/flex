#ifndef FLEX_DATAREQUESTMESSAGEHANDLER_H
#define FLEX_DATAREQUESTMESSAGEHANDLER_H


#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include "MessageHandlerWithOrphanage.h"
#include "TipRequestMessage.h"
#include "CreditSystem.h"
#include "Calendar.h"
#include "TipMessage.h"
#include "CalendarRequestMessage.h"


class DataMessageHandler : public MessageHandlerWithOrphanage
{
public:
    MemoryDataStore &msgdata, &creditdata;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};

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
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(TipRequestMessage);
        HANDLEHASH(TipMessage);
        HANDLEHASH(CalendarRequestMessage);
    }

    HANDLECLASS(TipRequestMessage);
    HANDLECLASS(TipMessage);
    HANDLECLASS(CalendarRequestMessage);

    void HandleTipRequestMessage(TipRequestMessage request);

    void HandleTipMessage(TipMessage message);

    void HandleCalendarRequestMessage(CalendarRequestMessage request);

    void SetCreditSystem(CreditSystem *credit_system_);

    void SetCalendar(Calendar *calendar_);

    uint160 RequestTips();

    void RequestCalendarOfTipWithTheMostWork();

    TipMessage TipWithTheMostWork();

    uint160 SendCalendarRequestToPeer(CNode *peer, MinedCredit mined_credit);

    std::vector<uint160> TipMessageHashesWithTheMostWork();

    void RecordReportedTotalWorkOfTip(uint160 tip_message_hash, uint160 total_work);

};


#endif //FLEX_DATAREQUESTMESSAGEHANDLER_H
