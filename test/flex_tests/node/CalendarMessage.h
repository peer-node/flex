#ifndef FLEX_CALENDARMESSAGE_H
#define FLEX_CALENDARMESSAGE_H


#include "CalendarRequestMessage.h"
#include "CreditSystem.h"
#include "Calendar.h"

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
        calendar = Calendar(request.credit_hash, credit_system);
    }

    static std::string Type() { return "calendar"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(calendar);
        READWRITE(request_hash);
    )

    JSON(calendar, request_hash);

    DEPENDENCIES();

    uint160 GetHash160()
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }
};


#endif //FLEX_CALENDARMESSAGE_H
