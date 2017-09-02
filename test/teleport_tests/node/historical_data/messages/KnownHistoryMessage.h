
#ifndef TELEPORT_KNOWNHISTORYMESSAGE_H
#define TELEPORT_KNOWNHISTORYMESSAGE_H


#include <string>
#include <src/crypto/uint256.h>
#include <test/teleport_tests/node/data_handler/messages/KnownHistoryRequest.h>
#include "KnownHistoryDeclaration.h"

class CreditSystem;
class Calendar;
class Calend;

class KnownHistoryMessage
{
public:
    uint160 request_hash{0};
    uint160 mined_credit_message_hash{0};
    KnownHistoryDeclaration history_declaration;

    KnownHistoryMessage() { }

    KnownHistoryMessage(KnownHistoryRequest request, CreditSystem *credit_system);

    void PopulateDeclaration(Calendar& calendar, CreditSystem* credit_system);

    void PopulateDataForCalend(Calend calend, uint64_t i, CreditSystem* credit_system);

    static std::string Type() { return "known_history"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(request_hash);
        READWRITE(mined_credit_message_hash);
        READWRITE(history_declaration);
    )

    JSON(request_hash, mined_credit_message_hash, history_declaration);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_KNOWNHISTORYMESSAGE_H
