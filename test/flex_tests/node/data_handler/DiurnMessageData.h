
#ifndef FLEX_DIURNMESSAGEDATA_H
#define FLEX_DIURNMESSAGEDATA_H

#include <src/crypto/uint256.h>
#include <src/base/version.h>
#include <src/crypto/hash.h>
#include "base/serialize.h"
#include "define.h"
#include "EnclosedMessageData.h"


class DiurnMessageData
{
public:
    EnclosedMessageData message_data;

    DiurnMessageData() { }

    static std::string Type() { return "diurn_message_data"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(message_data);
    );

    JSON(message_data);

    DEPENDENCIES();

    HASH160();
};


#endif //FLEX_DIURNMESSAGEDATA_H
