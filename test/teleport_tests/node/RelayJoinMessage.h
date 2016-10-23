
#ifndef TELEPORT_RELAYJOINMESSAGE_H
#define TELEPORT_RELAYJOINMESSAGE_H


#include <src/crypto/point.h>

class RelayJoinMessage
{
public:
    Point public_key{SECP256K1, 0};

    RelayJoinMessage() { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(public_key);
    )

    JSON(public_key);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_RELAYJOINMESSAGE_H
