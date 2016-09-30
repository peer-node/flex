#ifndef FLEX_DIURNDATAREQUEST_H
#define FLEX_DIURNDATAREQUEST_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include "define.h"


class DiurnDataRequest
{
public:
    uint160 msg_hash;
    std::vector<uint32_t> requested_diurns;

    DiurnDataRequest() { }

    DiurnDataRequest(uint160 msg_hash_, std::vector<uint32_t> requested_diurns_)
    {
        msg_hash = msg_hash_;
        requested_diurns = requested_diurns_;
    }

    static std::string Type() { return "diurn_data_request"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(msg_hash);
        READWRITE(requested_diurns);
    )

    DEPENDENCIES();

    HASH160();
};


#endif //FLEX_DIURNDATAREQUEST_H
