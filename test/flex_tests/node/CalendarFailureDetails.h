#ifndef FLEX_CALENDARFAILUREDETAILS_H
#define FLEX_CALENDARFAILUREDETAILS_H


#include <src/crypto/uint256.h>
#include <src/mining/work.h>
#include <src/net/net_cnode.h>

class CalendarFailureDetails
{
public:
    uint160 diurn_root;
    uint160 credit_hash;
    TwistWorkCheck check;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(diurn_root);
        READWRITE(credit_hash);
        READWRITE(check);
    )
};


#endif //FLEX_CALENDARFAILUREDETAILS_H
