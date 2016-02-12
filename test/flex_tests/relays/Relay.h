#ifndef FLEX_RELAY_H
#define FLEX_RELAY_H


#include <src/crypto/uint256.h>
#include "base/serialize.h"

class Relay
{
public:
    uint160 mined_credit_hash;
    uint64_t number;

    Relay(): number(0) { }

    IMPLEMENT_SERIALIZE
    (
    READWRITE(mined_credit_hash);
    );

};


#endif //FLEX_RELAY_H
