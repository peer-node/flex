#ifndef TELEPORT_TIPHANDLER_H
#define TELEPORT_TIPHANDLER_H

#include "DataMessageHandler.h"


class TipHandler
{
public:
    MemoryDataStore &msgdata, &creditdata;
    DataMessageHandler *data_message_handler;
    CreditSystem *credit_system;

    TipHandler(DataMessageHandler *data_message_handler_) :
        data_message_handler(data_message_handler_),
        msgdata(data_message_handler_->msgdata),
        creditdata(data_message_handler_->creditdata),
        credit_system(data_message_handler_->credit_system)
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


#endif //TELEPORT_TIPHANDLER_H
