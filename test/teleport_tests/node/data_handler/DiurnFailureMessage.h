#ifndef TELEPORT_DIURNFAILUREMESSAGE_H
#define TELEPORT_DIURNFAILUREMESSAGE_H


#include <src/define.h>
#include "DiurnFailureDetails.h"
#include "test/teleport_tests/node/calendar/Diurn.h"

class DiurnFailureMessage
{
public:
    DiurnFailureDetails details;
    Diurn diurn;

    DiurnFailureMessage() { }

    static std::string Type() { return "diurn_failure"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(details);
        READWRITE(diurn);
    )

    DEPENDENCIES(details.succeeding_calend_hash);

    JSON(details, diurn);

    HASH160();
};


#endif //TELEPORT_DIURNFAILUREMESSAGE_H
