#include "KnownHistoryMessage.h"
#include "test/teleport_tests/node/credit/structures/CreditSystem.h"

#include "log.h"
#define LOG_CATEGORY "KnownHistoryMessage.cpp"

KnownHistoryMessage::KnownHistoryMessage(KnownHistoryRequest request, CreditSystem *credit_system)
{
    request_hash = request.GetHash160();
    mined_credit_message_hash = request.mined_credit_message_hash;
    Calendar calendar(mined_credit_message_hash, credit_system);
    PopulateDeclaration(calendar, credit_system);
}

void KnownHistoryMessage::PopulateDeclaration(Calendar& calendar, CreditSystem* credit_system)
{
    history_declaration.known_diurns.SetLength(calendar.calends.size());

    for (uint64_t i = 0; i < calendar.calends.size(); i++)
        PopulateDataForCalend(calendar.calends[i], i, credit_system);
}

void KnownHistoryMessage::PopulateDataForCalend(Calend calend, uint64_t i, CreditSystem* credit_system)
{
    uint160 diurn_root = calend.mined_credit.network_state.DiurnRoot();
    if (credit_system->creditdata[diurn_root]["data_is_known"])
    {
        history_declaration.known_diurns.Set(i);

        uint160 stored_diurn_hash = credit_system->creditdata[diurn_root]["diurn_hash"];
        history_declaration.diurn_hashes.push_back(stored_diurn_hash);

        uint160 stored_message_data_hash = credit_system->creditdata[diurn_root]["message_data_hash"];
        history_declaration.diurn_message_data_hashes.push_back(stored_message_data_hash);
    }
}
