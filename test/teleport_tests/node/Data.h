#ifndef TELEPORT_DATA_H
#define TELEPORT_DATA_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>

class RelayState;


class Data
{
public:
    MemoryDataStore &msgdata, &creditdata, &keydata;
    RelayState *relay_state{NULL};

    Data(MemoryDataStore &msgdata, MemoryDataStore &creditdata, MemoryDataStore &keydata):
            msgdata(msgdata), creditdata(creditdata), keydata(keydata)
    { }

    Data(MemoryDataStore &msgdata, MemoryDataStore &creditdata, MemoryDataStore &keydata, RelayState *relay_state):
            msgdata(msgdata), creditdata(creditdata), keydata(keydata), relay_state(relay_state)
    { }

    template <typename T>
    void StoreMessage(T message)
    {
        msgdata[message.GetHash160()]["type"] = message.Type();
        msgdata[message.GetHash160()][message.Type()] = message;
    }
};



#endif //TELEPORT_DATA_H
