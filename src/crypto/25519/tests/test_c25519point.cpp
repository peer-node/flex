#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "curve25519point.h"
//#include "datastore.h"

static const uint8_t ed25519_order[F25519_SIZE] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

inline CBigNum F25519ToBigNum(const uint8_t *x)
{
    CBigNum number(0);
    for (int32_t i = F25519_SIZE - 1; i >=0 ; i--)
    {
        number <<= 8;
        uint64_t n = x[i];
        number = number + CBigNum(n);
    }
    return number;
}

int main(void)
{
    CBigNum n = 154645;
    CBigNum order = F25519ToBigNum(ed25519_order);

    uint8_t x_[F25519_SIZE];
    BigNumToF25519(order, x_);
    CBigNum order_ = F25519ToBigNum(x_);


    Curve25519Point p1(n);
    n <<= 2;

    Curve25519Point p2(n);

    p1 = p1 + 3 * p1;

    printf("p1 == p2: %d\n", p1 == p2);
    assert(p1 == p2);

    Curve25519Point p3, p4;



    p3 = order * p1;
    p4 = 0 * p1;

    assert(p4 == p3);    


	return 0;
}
