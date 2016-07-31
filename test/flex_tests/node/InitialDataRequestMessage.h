#ifndef FLEX_INITIALDATAREQUESTMESSAGE_H
#define FLEX_INITIALDATAREQUESTMESSAGE_H


#include <string>
#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include "CalendarMessage.h"

class InitialDataRequestMessage
{
public:
    uint160 latest_mined_credit_hash{0};

    InitialDataRequestMessage() { }

    InitialDataRequestMessage(CalendarMessage calendar_message)
    {
        latest_mined_credit_hash = calendar_message.calendar.LastMinedCreditHash();
    }

    static std::string Type() { return "initial_data_request"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(latest_mined_credit_hash);
    )

    DEPENDENCIES();

    HASH160();
};


#endif //FLEX_INITIALDATAREQUESTMESSAGE_H
