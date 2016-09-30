
#ifndef FLEX_KNOWNHISTORYHANDLER_H
#define FLEX_KNOWNHISTORYHANDLER_H

#include "DataMessageHandler.h"


class KnownHistoryHandler
{
public:
    MemoryDataStore &msgdata, &creditdata;
    DataMessageHandler *data_message_handler;

    KnownHistoryHandler(DataMessageHandler *data_message_handler_) :
        data_message_handler(data_message_handler_),
        msgdata(data_message_handler_->msgdata),
        creditdata(data_message_handler_->creditdata)
    { }

    void HandleKnownHistoryMessage(KnownHistoryMessage known_history);

    KnownHistoryMessage GenerateKnownHistoryMessage();

    bool ValidateKnownHistoryMessage(KnownHistoryMessage known_history_message);

    void HandleKnownHistoryRequest(KnownHistoryRequest request);

    uint160 RequestKnownHistoryMessages(uint160 mined_credit_message_hash);

    uint160 RequestDiurnData(uint160 msg_hash, std::vector<uint32_t> requested_diurns);

    uint160 RequestDiurnData(uint160 msg_hash, std::vector<uint32_t> requested_diurns, CNode *peer);

    void HandleDiurnDataMessage(DiurnDataMessage diurn_data_message);

    void HandlerDiurnDataRequest(DiurnDataRequest request);
};


#endif //FLEX_KNOWNHISTORYHANDLER_H
