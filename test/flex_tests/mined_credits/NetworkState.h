#ifndef FLEX_NETWORKSTATE_H
#define FLEX_NETWORKSTATE_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>

class NetworkState
{
public:
    uint256 network_id;
    uint160 previous_mined_credit_hash;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(network_id);
    )
};


#endif //FLEX_NETWORKSTATE_H
