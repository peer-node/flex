#include <stdio.h>
#include <string.h>
#include "25519points.h"
#include <string>


void print_num(const unsigned char *x)
{
    uint32_t i;
    for (i = 0; i < 32; i++)
         printf("%0X ", x[i]);
}

void BigNum256ModMFromU64(bignum256modm bignum, uint64_t number)
{
    memset(bignum, 0, 40);
    bignum[0] = number & 0xffffffffffffff;
    bignum[1] = number >> 56;
}


CBigNum BigNumFromBigNum256ModM(bignum25519 bignum)
{
    CBigNum number(0);
    number += bignum[4] & 0xffffffff;
    for (int32_t i = 3; i >= 0; i--)
    {
        number <<= 56;
        number += bignum[i] & 0xffffffffffffff;
    }
    return number;
}


static void
test_main(void) {
  bignum25519 order;

  BigNum256ModMFromBigNum(order, F25519ToBigNum(ed25519_order));

  bignum256modm three;
  BigNum256ModMFromU64(three, 3);
  
  for (uint32_t k = 0; k < 2000; k++)
  {
    bignum25519 num1, num2;
    BigNum256ModMFromU64(num1, (154647 * k));
    
    Ed25519Point a_(num1), c(num1);

    a_ += c;    
    a_ += c;
    c *= three;

    assert(c == a_);

    c *= order;

    bignum256modm zero;
    BigNum256ModMFromU64(zero, 0);
    a_ *= zero;

    assert(c == a_);
  }
}


static void
test_main_curve25519(void) {
  bignum25519 order;

  BigNum256ModMFromBigNum(order, F25519ToBigNum(ed25519_order));

  bignum256modm three;
  BigNum256ModMFromU64(three, 3);
  bignum256modm zero;
  BigNum256ModMFromU64(zero, 0);
  
  bignum25519 num1, num2;
  
  for (uint32_t k = 0; k < 500; k++)
  {
    BigNum256ModMFromU64(num1, (154647 * k));
    
    Curve25519Point a_(num1), c(a_);

    a_ += c;    
    a_ += c;
    c *= three;

    assert(c == a_);

    c *= order;
    a_ *= zero;

    assert(c == a_);
  }
}


int
main(void) {
    test_main();
    test_main_curve25519();
    return 0;
}

