
#ifndef FLEX_KNOWNHISTORYMESSAGE_H
#define FLEX_KNOWNHISTORYMESSAGE_H


#include <string>
#include <src/crypto/uint256.h>
#include "test/flex_tests/node/CreditSystem.h"
#include "KnownHistoryDeclaration.h"

class KnownHistoryMessage
{
public:
    uint160 latest_mined_credit_message_hash{0};
    KnownHistoryDeclaration history_declaration;

    KnownHistoryMessage() { }

    KnownHistoryMessage(uint160 msg_hash, CreditSystem *credit_system)
    {
        latest_mined_credit_message_hash = msg_hash;
        Calendar calendar(msg_hash, credit_system);
        PopulateDeclaration(calendar, credit_system);
    }

    void PopulateDeclaration(Calendar& calendar, CreditSystem* credit_system)
    {
        history_declaration.known_diurns.SetLength(calendar.calends.size());
        history_declaration.known_diurn_message_data.SetLength(calendar.calends.size());

        for (uint64_t i = 0; i < calendar.calends.size(); i++)
            PopulateDataForCalend(calendar.calends[i], i, credit_system);
    }

    void PopulateDataForCalend(Calend calend, uint64_t i, CreditSystem* credit_system)
    {
        uint160 diurn_root = calend.mined_credit.network_state.DiurnRoot();
        if (credit_system->creditdata[diurn_root]["data_is_known"])
        {
            history_declaration.known_diurns.Set(i);
            uint160 stored_diurn_hash = credit_system->creditdata[diurn_root]["diurn_hash"];
            history_declaration.diurn_hashes.push_back(stored_diurn_hash);
        }
        if (credit_system->creditdata[diurn_root]["message_data_is_known"])
        {
            history_declaration.known_diurn_message_data.Set(i);
            uint160 stored_message_data_hash = credit_system->creditdata[diurn_root]["message_data_hash"];
            history_declaration.diurn_message_data_hashes.push_back(stored_message_data_hash);
        }
    }

    static std::string Type() { return "known_history"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(history_declaration);
    )

    DEPENDENCIES();

    HASH160();
};


#endif //FLEX_KNOWNHISTORYMESSAGE_H
