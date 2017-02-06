#ifndef TELEPORT_DATA_H
#define TELEPORT_DATA_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include <src/vector_tools.h>

class RelayState;
class RelayMemoryCache;


class Data
{
public:
    MemoryDataStore &msgdata, &creditdata, &keydata;
    RelayState *relay_state{NULL};
    RelayMemoryCache *cache{NULL};
    bool using_internal_cache{true};


    Data(MemoryDataStore &msgdata, MemoryDataStore &creditdata, MemoryDataStore &keydata):
            msgdata(msgdata), creditdata(creditdata), keydata(keydata)
    { }

    Data(MemoryDataStore &msgdata, MemoryDataStore &creditdata, MemoryDataStore &keydata, RelayState *relay_state):
            msgdata(msgdata), creditdata(creditdata), keydata(keydata), relay_state(relay_state)
    { }

    Data(const Data &other): msgdata(other.msgdata), creditdata(other.creditdata), keydata(other.keydata)
    {
        relay_state = other.relay_state;
        cache = other.cache;
        using_internal_cache = false;
    }

    ~Data();

    void CreateCache();

    template <typename T>
    void StoreMessage(T message)
    {
        uint160 message_hash = message.GetHash160();
        msgdata[message_hash]["type"] = message.Type();
        msgdata[message_hash][message.Type()] = message;
        msgdata[message_hash]["received"] = true;
        StoreHash(message_hash);
    }

    MemoryProperty &GetMessage(uint160 message_hash)
    {
        return msgdata[message_hash][MessageType(message_hash)];
    }

    std::string MessageType(uint160 message_hash)
    {
        return msgdata[message_hash]["type"];
    }

    void StoreHash(uint160 hash)
    {
        uint32_t short_hash = *(uint32_t*)&hash;
        std::vector<uint160> matches = msgdata[short_hash]["matches"];
        if (!VectorContainsEntry(matches, hash))
            matches.push_back(hash);
        msgdata[short_hash]["matches"] = matches;
    }

    RelayState GetRelayState(uint160 relay_state_hash);

    void StoreRelayState(RelayState *relay_state);
};


#endif //TELEPORT_DATA_H
