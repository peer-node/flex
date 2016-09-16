#ifndef FLEX_CALENDARFAILUREDETAILS_H
#define FLEX_CALENDARFAILUREDETAILS_H


#include <src/crypto/uint256.h>
#include <src/mining/work.h>
#include <src/net/net_cnode.h>

class CalendarFailureDetails
{
public:
    uint160 diurn_root{0};
    uint160 credit_hash{0};
    uint160 mined_credit_message_hash{0};
    TwistWorkCheck check;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(diurn_root);
        READWRITE(credit_hash);
        READWRITE(mined_credit_message_hash);
        READWRITE(check);
    )

    JSON(diurn_root, credit_hash, mined_credit_message_hash, check);
};


#endif //FLEX_CALENDARFAILUREDETAILS_H
