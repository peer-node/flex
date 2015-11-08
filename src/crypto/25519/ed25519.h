#ifndef ED25519_H
#define ED25519_H

#include <stdlib.h>
#include "crypto/bignum.h"

typedef uint64_t bignum25519[5];
typedef uint64_t bignum256modm_element_t;
typedef bignum256modm_element_t bignum256modm[5];

#ifndef GE25519
#define GE25519

typedef struct ge25519_t {
    bignum25519 x, y, z, t;
} ge25519;

typedef struct ge25519_p1p1_t {
    bignum25519 x, y, z, t;
} ge25519_p1p1;

typedef struct ge25519_niels_t {
    bignum25519 ysubx, xaddy, t2d;
} ge25519_niels;

typedef struct ge25519_pniels_t {
    bignum25519 ysubx, xaddy, z, t2d;
} ge25519_pniels;

#endif

CBigNum bignum25519toCBigNum(const bignum25519 n);
void CBigNumtobignum25519(bignum25519 n, CBigNum number);

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned char ed25519_signature[64];
typedef unsigned char ed25519_public_key[32];
typedef unsigned char ed25519_secret_key[32];

typedef unsigned char curved25519_key[32];

void mod25519(bignum25519 n);
void printhex_(const bignum256modm a);
void ed25519_publickey(const ed25519_secret_key sk, ed25519_public_key pk);
void ed25519_publickey_from_number(const ed25519_secret_key sk, ed25519_public_key pk);
int ed25519_sign_open(const unsigned char *m, size_t mlen, const ed25519_public_key pk, const ed25519_signature RS);
void ed25519_sign(const unsigned char *m, size_t mlen, const ed25519_secret_key sk, const ed25519_public_key pk, ed25519_signature RS);

int ed25519_sign_open_batch(const unsigned char **m, size_t *mlen, const unsigned char **pk, const unsigned char **RS, size_t num, int *valid);

void ed25519_randombytes_unsafe(void *out, size_t count);

void curved25519_scalarmult_basepoint(curved25519_key pk, const curved25519_key e);

void morph25519_e2m(bignum25519 x_out, const bignum25519 ey, const bignum256modm ez);
void mx2ey(bignum25519 ey, bignum25519 mx);
void F25519toBigNum25519(bignum25519 x, const uint8_t *d);
static uint8_t ey2ex(bignum25519 x, bignum25519 y, int parity);
int morph25519_eparity(bignum25519 edwards_x);
uint8_t morph25519_m2e(bignum25519 ex, bignum25519 ey,
                       bignum25519 mx, int parity);

void curved25519_scalarmult(curved25519_key pkout,
                                    int* parity_out,
                                    curved25519_key pkin,
                                    int parity,
                                    const bignum256modm multiplicand);
void ed25519_project(ge25519 *p, bignum25519 x, bignum25519 y);
void ed25519_unproject(bignum25519 x, bignum25519 y, const ge25519 *p);
void curved25519_add(curved25519_key pkout,
                             int* parity_out,
                             const curved25519_key pkin1,
                             const int parity1,
                             const curved25519_key pkin2,
                             const int parity2);

#if defined(__cplusplus)
}
#endif

#endif // ED25519_H
