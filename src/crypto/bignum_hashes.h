#ifndef FLEX_BIGNUM_HASHES_H
#define FLEX_BIGNUM_HASHES_H

#include "crypto/point.h"

inline CBigNum Hash(Point point)
{
    vch_t bytes = point.getvch();
    return CBigNum(Hash(bytes.begin(), bytes.end())) % point.Modulus();
}

inline CBigNum Hash(CBigNum number)
{
    vch_t bytes = number.getvch();
    return CBigNum(Hash(bytes.begin(), bytes.end()));
}



#endif //FLEX_BIGNUM_HASHES_H
