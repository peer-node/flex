#ifndef TELEPORT_CALENDARFAILUREDETAILS_H
#define TELEPORT_CALENDARFAILUREDETAILS_H


#include <src/crypto/uint256.h>
#include <src/mining/work.h>
#include <src/net/net_cnode.h>

class CalendarFailureDetails
{
public:
    uint160 diurn_root{0};
    uint160 mined_credit_message_hash{0};
    TwistWorkCheck check;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(diurn_root);
        READWRITE(mined_credit_message_hash);
        READWRITE(check);
    )

    JSON(diurn_root, mined_credit_message_hash, check);
};


#endif //TELEPORT_CALENDARFAILUREDETAILS_H
