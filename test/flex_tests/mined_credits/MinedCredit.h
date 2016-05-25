#ifndef FLEX_MINEDCREDIT_H
#define FLEX_MINEDCREDIT_H

#include <src/crypto/point.h>
#include "credits/Credit.h"
#include "EncodedNetworkState.h"


class MinedCredit : public Credit
{
public:
    EncodedNetworkState network_state;

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

    Point PublicKey()
    {
        if (keydata.size() == 34)
        {
            Point public_key;
            public_key.setvch(keydata);
            return public_key;
        }
        return Point();
    }

    bool operator==(const MinedCredit& other) const
    {
        return amount == other.amount and keydata == other.keydata and network_state == other.network_state;
    }
};


#endif //FLEX_MINEDCREDIT_H
