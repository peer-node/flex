#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ed25519point.h"

static const uint8_t ed25519_order[ED25519_EXPONENT_SIZE] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};


static void print_elem(const uint8_t *e)
{
    int i;

    for (i = 0; i < F25519_SIZE; i++)
        printf("%02x", e[i]);
}

int main(void)
{
    CBigNum order = F25519ToBigNum(ed25519_order);
    char buf[BUFSIZ];
    CBigNum signature, num, x, y;

    for (uint32_t i = 1; i < 200; i++)
    {
        CBigNum n(1);
        
        for (uint32_t j = 0; j < i; j += 1)
        {
            n = (n * CBigNum(3)) % order;
        }
        printf("hex: %s\n", n.GetHex().c_str());
        Ed25519Point p(n);
        p.GetCoordinates(x, y);
        printf("point = %s,%s\n", x.ToString().c_str(), 
                                         y.ToString().c_str());
        printf("ok: %d\n", (p + p == Ed25519Point((2 * n) % order)));
    }


    printf("sig: ");
    fflush(stdout);
    scanf("%s", buf);
    signature.SetHex(buf);
    printf("num: ");
    fflush(stdout);
    scanf("%s", buf);
    num.SetHex(buf);

    Ed25519Point p_one(CBigNum(1));
    p_one.GetCoordinates(x, y);

    Ed25519Point psig(signature);
    psig.GetCoordinates(x, y);
    printf("signature = %s\n", signature.ToString().c_str());
    printf("point(signature) = %s,%s\n", x.ToString().c_str(), 
                                         y.ToString().c_str());
    Ed25519Point pnum(num);
    pnum.GetCoordinates(x, y);
    printf("point(num) = %s,%s\n", x.ToString().c_str(), 
                                         y.ToString().c_str());
    Ed25519Point psigplusnum(signature + num);
    psigplusnum.GetCoordinates(x, y);
    printf("point(signature + num) = %s,%s\n", x.ToString().c_str(), 
                                         y.ToString().c_str());

    assert(psig + pnum == psigplusnum);    

    return 0;
}
