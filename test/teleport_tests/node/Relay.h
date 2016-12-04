#ifndef TELEPORT_RELAY_H
#define TELEPORT_RELAY_H


#include <src/crypto/point.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "RelayJoinMessage.h"
#include "KeyDistributionMessage.h"


class Relay
{
public:
    Point public_key{SECP256K1, 0};
    uint64_t number{0};
    uint160 join_message_hash{0};
    std::vector<uint64_t> quarter_key_holders;
    std::vector<uint64_t> first_set_of_key_sixteenth_holders;
    std::vector<uint64_t> second_set_of_key_sixteenth_holders;
    std::vector<Point> public_key_sixteenths;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(public_key);
        READWRITE(number);
        READWRITE(join_message_hash);
        READWRITE(quarter_key_holders);
        READWRITE(first_set_of_key_sixteenth_holders);
        READWRITE(second_set_of_key_sixteenth_holders);
    )

    JSON(public_key, number, join_message_hash, quarter_key_holders,
         first_set_of_key_sixteenth_holders, second_set_of_key_sixteenth_holders);

    RelayJoinMessage GenerateJoinMessage(MemoryDataStore &keydata, uint160 mined_credit_message_hash);

    std::vector<std::vector<uint64_t> > KeyPartHolderGroups();

    KeyDistributionMessage
    GenerateKeyDistributionMessage(Databases databases, uint160 encoding_message_hash, RelayState &state);

    std::vector<CBigNum> PrivateKeySixteenths(MemoryDataStore &keydata);
};


#endif //TELEPORT_RELAY_H
