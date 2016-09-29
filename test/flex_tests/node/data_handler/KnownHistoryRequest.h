#ifndef FLEX_KNOWNHISTORYREQUEST_H
#define FLEX_KNOWNHISTORYREQUEST_H


#include <src/define.h>
#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include <src/base/util_time.h>
#include <src/base/version.h>
#include <src/crypto/hash.h>

class KnownHistoryRequest
{
public:
    uint160 mined_credit_message_hash{0};
    uint64_t timestamp{0};

    KnownHistoryRequest() { }

    KnownHistoryRequest(uint160 msg_hash)
    {
        mined_credit_message_hash = msg_hash;
        timestamp = GetTimeMicros();
    }

    static std::string Type() { return "known_history_request"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit_message_hash);
        READWRITE(timestamp);
    );

    JSON(mined_credit_message_hash, timestamp);

    DEPENDENCIES();

    HASH160();
};


#endif //FLEX_KNOWNHISTORYREQUEST_H
