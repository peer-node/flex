#ifndef TELEPORT_RELAY_H
#define TELEPORT_RELAY_H


#include <src/crypto/point.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "KeyQuartersMessage.h"

class Relay
{
public:
    Point public_key{SECP256K1, 0};
    uint64_t number{0};
    uint160 join_message_hash{0};
    std::vector<uint64_t> quarter_key_holders;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(public_key);
        READWRITE(number);
        READWRITE(join_message_hash);
        READWRITE(quarter_key_holders);
    )

    JSON(public_key, number, join_message_hash, quarter_key_holders);

    KeyQuartersMessage GenerateKeyQuartersMessage(MemoryDataStore &keydata);
};


#endif //TELEPORT_RELAY_H
