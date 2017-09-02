#ifndef TELEPORT_TIPMESSAGE_H
#define TELEPORT_TIPMESSAGE_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include "TipRequestMessage.h"
#include "test/teleport_tests/node/calendar/Calendar.h"

class TipMessage
{
public:
    TipMessage() { }

    TipMessage(TipRequestMessage tip_request, Calendar *calendar);

    static std::string Type() { return "tip"; }

    MinedCreditMessage mined_credit_message;
    uint160 tip_request_hash;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit_message);
        READWRITE(tip_request_hash);
    );

    JSON(mined_credit_message, tip_request_hash);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_TIPMESSAGE_H
