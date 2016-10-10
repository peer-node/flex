#include "DiurnDataMessage.h"

#include "log.h"
#define LOG_CATEGORY "DiurnDataMessage.cpp"

DiurnDataMessage::DiurnDataMessage(DiurnDataRequest request, CreditSystem *credit_system)
{
    request_hash = request.GetHash160();
    PopulateDiurnData(request, credit_system);
}

void DiurnDataMessage::PopulateDiurnData(DiurnDataRequest request, CreditSystem *credit_system)
{
    Calendar calendar(request.msg_hash, credit_system);

    for (auto diurn_number : request.requested_diurns)
    {
        Diurn diurn = credit_system->PopulateDiurnPrecedingCalend(calendar.calends[diurn_number].GetHash160());
        diurns.push_back(diurn);
        calends.push_back(calendar.calends[diurn_number]);

        DiurnMessageData data = calendar.calends[diurn_number].GenerateDiurnMessageData(credit_system);
        message_data.push_back(data);

        BitChain spent_chain = credit_system->GetSpentChain(diurn.credits_in_diurn[0].GetHash160());
        initial_spent_chains.push_back(spent_chain);
    }
}
