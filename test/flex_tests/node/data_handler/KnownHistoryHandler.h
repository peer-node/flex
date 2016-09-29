
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

    KnownHistoryMessage GenerateKnownHistoryMessage();

    bool ValidateKnownHistoryMessage(KnownHistoryMessage known_history_message);
};


#endif //FLEX_KNOWNHISTORYHANDLER_H
