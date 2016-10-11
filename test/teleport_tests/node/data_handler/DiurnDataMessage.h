
#ifndef TELEPORT_DIURNDATAMESSAGE_H
#define TELEPORT_DIURNDATAMESSAGE_H


#include "test/teleport_tests/node/calendar/Diurn.h"
#include "DiurnDataRequest.h"
#include "DiurnMessageData.h"
#include "test/teleport_tests/node/CreditSystem.h"

class DiurnDataMessage
{
public:
    uint160 request_hash;
    std::vector<Diurn> diurns;
    std::vector<Calend> calends;
    std::vector<BitChain> initial_spent_chains;
    std::vector<DiurnMessageData> message_data;

    DiurnDataMessage() { }

    DiurnDataMessage(DiurnDataRequest request, CreditSystem *credit_system);

    void PopulateDiurnData(DiurnDataRequest request, CreditSystem *credit_system);

    static std::string Type() { return "diurn_data"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(request_hash);
        READWRITE(diurns);
        READWRITE(calends);
        READWRITE(initial_spent_chains);
        READWRITE(message_data);
    )

    JSON(request_hash, diurns, calends, initial_spent_chains, message_data);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_DIURNDATAMESSAGE_H
