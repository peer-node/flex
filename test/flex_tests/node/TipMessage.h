#ifndef FLEX_TIPMESSAGE_H
#define FLEX_TIPMESSAGE_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include "TipRequestMessage.h"
#include "Calendar.h"

class TipMessage
{
public:
    TipMessage() { }

    TipMessage(TipRequestMessage tip_request, Calendar *calendar);

    static std::string Type() { return "tip"; }

    MinedCredit mined_credit;
    uint160 tip_request_hash;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit);
        READWRITE(tip_request_hash);
    );

    DEPENDENCIES();

    uint160 GetHash160()
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }
};


#endif //FLEX_TIPMESSAGE_H
