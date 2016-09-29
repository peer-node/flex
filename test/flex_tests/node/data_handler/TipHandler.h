#ifndef FLEX_TIPHANDLER_H
#define FLEX_TIPHANDLER_H

#include "DataMessageHandler.h"


class TipHandler
{
public:
    MemoryDataStore &msgdata, &creditdata;
    DataMessageHandler *data_message_handler;

    TipHandler(DataMessageHandler *data_message_handler_) :
        data_message_handler(data_message_handler_),
        msgdata(data_message_handler_->msgdata),
        creditdata(data_message_handler_->creditdata)
    { }

    void HandleTipRequestMessage(TipRequestMessage request);

    void HandleTipMessage(TipMessage message);

    uint160 RequestTips();

    uint160 RequestCalendarOfTipWithTheMostWork();

    uint160 SendCalendarRequestToPeer(CNode *peer, MinedCreditMessage msg);

    TipMessage TipWithTheMostWork();

    std::vector<uint160> TipMessageHashesWithTheMostReportedWork();

    void RecordReportedTotalWorkOfTip(uint160 tip_message_hash, uint160 total_work);

};


#endif //FLEX_TIPHANDLER_H
