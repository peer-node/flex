#ifndef TELEPORT_GOODBYEMESSAGE_H
#define TELEPORT_GOODBYEMESSAGE_H


#include <src/crypto/signature.h>
#include "test/teleport_tests/node/Data.h"

class Relay;


class GoodbyeMessage
{
public:
    uint64_t dead_relay_number{0};
    Signature signature;

    static std::string Type() { return "goodbye"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(dead_relay_number);
        READWRITE(signature);
    );

    JSON(dead_relay_number, signature);

    DEPENDENCIES();

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);
};


#endif //TELEPORT_GOODBYEMESSAGE_H
