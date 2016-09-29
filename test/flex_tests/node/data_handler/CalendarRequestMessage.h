#ifndef FLEX_CALENDARREQUESTMESSAGE_H
#define FLEX_CALENDARREQUESTMESSAGE_H


#include "test/flex_tests/node/MinedCreditMessage.h"

class CalendarRequestMessage
{
public:
    uint160 msg_hash;

    CalendarRequestMessage() { }

    CalendarRequestMessage(MinedCreditMessage msg)
    {
        msg_hash = msg.GetHash160();
    }

    static std::string Type() { return "calendar_request"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(msg_hash);
    )

    DEPENDENCIES();

    HASH160();
};


#endif //FLEX_CALENDARREQUESTMESSAGE_H
