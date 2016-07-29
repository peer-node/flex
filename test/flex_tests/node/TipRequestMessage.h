#ifndef FLEX_TIPREQUESTMESSAGE_H
#define FLEX_TIPREQUESTMESSAGE_H


#include <string>
#include <src/base/serialize.h>
#include <src/crypto/uint256.h>
#include <src/base/version.h>
#include <src/define.h>
#include <src/crypto/hash.h>
#include <src/base/util_time.h>

class TipRequestMessage
{
public:
    static std::string Type() { return "tip_request"; }

    uint64_t timestamp{0};

    TipRequestMessage()
    {
       timestamp = GetTimeMicros();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(timestamp);
    );

    DEPENDENCIES();

    uint160 GetHash160()
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }
};


#endif //FLEX_TIPREQUESTMESSAGE_H
