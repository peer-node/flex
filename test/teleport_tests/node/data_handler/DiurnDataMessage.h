
#ifndef TELEPORT_DIURNDATAMESSAGE_H
#define TELEPORT_DIURNDATAMESSAGE_H


#include "test/teleport_tests/node/Diurn.h"
#include "DiurnDataRequest.h"
#include "test/teleport_tests/node/CreditSystem.h"

class DiurnDataMessage
{
public:
    uint160 request_hash;
    std::vector<Diurn> diurns;

    DiurnDataMessage() { }

    DiurnDataMessage(DiurnDataRequest request, CreditSystem *credit_system)
    {
        request_hash = request.GetHash160();
        PopulateDiurnData(request, credit_system);
    }

    void PopulateDiurnData(DiurnDataRequest request, CreditSystem *credit_system)
    {
        Calendar calendar(request.msg_hash, credit_system);

        for (auto diurn_number : request.requested_diurns)
        {
            Diurn diurn = credit_system->PopulateDiurn(calendar.calends[diurn_number].GetHash160());
            diurns.push_back(diurn);
        }
    }

    static std::string Type() { return "diurn_data"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(request_hash);
        READWRITE(diurns);
    )

    JSON(request_hash, diurns);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_DIURNDATAMESSAGE_H
