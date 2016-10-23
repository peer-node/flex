#ifndef TELEPORT_KEYQUARTERSMESSAGE_H
#define TELEPORT_KEYQUARTERSMESSAGE_H


#include <src/crypto/point.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>

class KeyQuartersMessage
{
public:
    uint160 join_message_hash{0};
    std::vector<Point> public_key_quarters;

    KeyQuartersMessage() { }

    static std::string Type() { return "key_quarters"; }

    void SetQuartersAndStoreCorrespondingSecrets(Point public_key, MemoryDataStore &keydata);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(public_key_quarters);
        READWRITE(join_message_hash);
    )

    JSON(public_key_quarters, join_message_hash);

    DEPENDENCIES();

    HASH160();

    void AddQuarterKeyAndStoreCorrespondingSecret(CBigNum secret, MemoryDataStore &keydata);
};


#endif //TELEPORT_KEYQUARTERSMESSAGE_H
