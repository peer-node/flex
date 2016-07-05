#ifndef FLEX_CALENDARREQUESTMESSAGE_H
#define FLEX_CALENDARREQUESTMESSAGE_H


#include <test/flex_tests/mined_credits/MinedCredit.h>

class CalendarRequestMessage
{
public:
    uint160 credit_hash;

    CalendarRequestMessage() { }

    CalendarRequestMessage(MinedCredit mined_credit)
    {
        credit_hash = mined_credit.GetHash160();
    }

    static std::string Type() { return "calendar_request"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_hash);
    )

    DEPENDENCIES();

    uint160 GetHash160();
};


#endif //FLEX_CALENDARREQUESTMESSAGE_H
