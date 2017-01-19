#ifndef TELEPORT_RELAYKEYHOLDERDATA_H
#define TELEPORT_RELAYKEYHOLDERDATA_H


#include <src/define.h>
#include <test/teleport_tests/teleport_data/MemoryObject.h>
#include <src/crypto/uint256.h>

class RelayKeyHolderData
{
public:
    std::vector<uint64_t> key_quarter_holders;
    std::vector<uint64_t> first_set_of_key_sixteenth_holders;
    std::vector<uint64_t> second_set_of_key_sixteenth_holders;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(key_quarter_holders);
        READWRITE(first_set_of_key_sixteenth_holders);
        READWRITE(second_set_of_key_sixteenth_holders);
    );

    JSON(key_quarter_holders, first_set_of_key_sixteenth_holders, second_set_of_key_sixteenth_holders);

    HASH160();

    void Store(MemoryObject &object);

    void Retrieve(uint160 holder_data_hash, MemoryObject &object);
};


#endif //TELEPORT_RELAYKEYHOLDERDATA_H
