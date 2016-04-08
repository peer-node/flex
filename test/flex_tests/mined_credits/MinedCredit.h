#ifndef FLEX_MINEDCREDIT_H
#define FLEX_MINEDCREDIT_H


#include <src/base/serialize.h>
#include <src/base/version.h>
#include <src/crypto/hash.h>
#include "credits/Credit.h"
#include "NetworkState.h"


class MinedCredit : public Credit
{
public:
    NetworkState network_state;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(keydata);
        READWRITE(network_state);
    )

    uint160 GetHash160()
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }

};


#endif //FLEX_MINEDCREDIT_H
