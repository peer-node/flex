#ifndef TELEPORT_CALENDARREQUESTMESSAGE_H
#define TELEPORT_CALENDARREQUESTMESSAGE_H


#include "test/teleport_tests/node/credit/messages/MinedCreditMessage.h"

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


#endif //TELEPORT_CALENDARREQUESTMESSAGE_H
