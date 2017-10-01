#ifndef TELEPORT_TIMEANDRECIPIENT_H
#define TELEPORT_TIMEANDRECIPIENT_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>

class TimeAndRecipient
{
public:
    uint256 data;

    TimeAndRecipient() { }

    TimeAndRecipient(uint64_t time_, uint160 recipient)
    {
        data = time_;
        data <<= 160;
        memcpy((char*)&data, (char*)&recipient, 20);
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(data);
    )

    uint64_t Time()
    {
        uint256 data_ = data;
        data_ >>= 160;
        return data_.GetLow64();
    }

    uint160 Recipient()
    {
        uint160 recipient;
        memcpy((char*)&recipient, (char*)&data, 20);
        return recipient;
    }
};

#endif //TELEPORT_TIMEANDRECIPIENT_H
