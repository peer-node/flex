#ifndef TELEPORT_CALENDARMESSAGE_H
#define TELEPORT_CALENDARMESSAGE_H


#include "CalendarRequestMessage.h"
#include "test/teleport_tests/node/credit/structures/CreditSystem.h"
#include "test/teleport_tests/node/calendar/Calendar.h"

#include "log.h"
#define LOG_CATEGORY "CalendarMessage.h"

class CalendarMessage
{
public:
    Calendar calendar;
    uint160 request_hash;

    CalendarMessage() { }

    CalendarMessage(CalendarRequestMessage request, CreditSystem *credit_system)
    {
        request_hash = request.GetHash160();
        calendar = Calendar(request.msg_hash, credit_system);
    }

    static std::string Type() { return "calendar"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(calendar);
        READWRITE(request_hash);
    )

    JSON(calendar, request_hash);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_CALENDARMESSAGE_H
