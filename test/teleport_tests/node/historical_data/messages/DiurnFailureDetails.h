#ifndef TELEPORT_DIURNFAILUREDETAILS_H
#define TELEPORT_DIURNFAILUREDETAILS_H


#include <src/crypto/uint256.h>
#include <src/mining/work.h>

class DiurnFailureDetails
{
public:
    uint160 preceding_calend_hash{0};
    uint160 mined_credit_message_hash{0};
    uint160 succeeding_calend_hash{0};
    TwistWorkCheck check;

    DiurnFailureDetails() { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(preceding_calend_hash);
        READWRITE(mined_credit_message_hash);
        READWRITE(succeeding_calend_hash);
        READWRITE(check);
    )

    JSON(preceding_calend_hash, mined_credit_message_hash, succeeding_calend_hash, check);
};


#endif //TELEPORT_DIURNFAILUREDETAILS_H
