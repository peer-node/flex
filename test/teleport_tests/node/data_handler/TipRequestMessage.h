#ifndef TELEPORT_TIPREQUESTMESSAGE_H
#define TELEPORT_TIPREQUESTMESSAGE_H


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

    JSON(timestamp);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_TIPREQUESTMESSAGE_H
