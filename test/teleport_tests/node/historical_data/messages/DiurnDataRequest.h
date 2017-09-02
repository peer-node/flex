#ifndef TELEPORT_DIURNDATAREQUEST_H
#define TELEPORT_DIURNDATAREQUEST_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include "define.h"
#include "KnownHistoryMessage.h"


class DiurnDataRequest
{
public:
    uint160 msg_hash{0};
    uint160 known_history_message_hash{0};
    std::vector<uint32_t> requested_diurns;

    DiurnDataRequest() { }

    DiurnDataRequest(KnownHistoryMessage history_message, std::vector<uint32_t> requested_diurns_)
    {
        known_history_message_hash = history_message.GetHash160();
        msg_hash = history_message.mined_credit_message_hash;
        requested_diurns = requested_diurns_;
    }

    static std::string Type() { return "diurn_data_request"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(msg_hash);
        READWRITE(known_history_message_hash);
        READWRITE(requested_diurns);
    )

    JSON(msg_hash, known_history_message_hash, requested_diurns);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_DIURNDATAREQUEST_H
