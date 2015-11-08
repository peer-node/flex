/*
	Public domain by Andrew M. <liquidsun@gmail.com>

	Ed25519 reference implementation using Ed25519-donna
*/


/* define ED25519_SUFFIX to have it appended to the end of each public function */
#if !defined(ED25519_SUFFIX)
#define ED25519_SUFFIX 
#endif

#define ED25519_FN3(fn,suffix) fn##suffix
#define ED25519_FN2(fn,suffix) ED25519_FN3(fn,suffix)
#define ED25519_FN(fn)         ED25519_FN2(fn,ED25519_SUFFIX)

#include "bignum.h"
#include "ed25519-donna.h"
#include "ed25519.h"
#include "ed25519-randombytes.h"
#include "ed25519-hash.h"
#include <stdio.h>


#define F25519_SIZE 32


#ifndef BN25519BIGNUM
#define BN25519BIGNUM

CBigNum bignum25519toCBigNum(const bignum25519 n)
{
    unsigned char r[32];
    curve25519_contract(r, n);
    return CBigNum(std::vector<unsigned char>(&r[0], &r[32]));
}

void CBigNumtobignum25519(bignum25519 n, CBigNum number)
{
    CBigNum m = number;
    unsigned char r[32];
    for (int32_t i = 0; i <32; i++)
    {
        r[i] = (m % 256).getulong();
        m >>= 8;
    }
    curve25519_expand(n, r);
}

#endif


void printhex_(const bignum25519 a)
{
    unsigned char r[32];
    curve25519_contract(r, a);
    for (uint32_t i = 0; i < 32; i++)
        printf("%02x", r[i]);
    printf("\n");
}


void mod25519(bignum25519 n)
{
    // printf("mod25519: n starts at ");
    // printhex_(n);
    // return;
    CBigNum bignumn = bignum25519toCBigNum(n);
    // printf("mod25519: biognumn is %s\n",
    //      bignumn.ToString().c_str());
    CBigNum modulus(1);
    modulus <<= 255;
    modulus = modulus - 19;
    bignumn = bignumn  % modulus;
    // printf("mod25519: biognumn is %s\n",
    //      bignumn.ToString().c_str());
    CBigNumtobignum25519(n, bignumn);
    // printf("mod25519: n is now ");
    // printhex_(n);
    bignumn = bignum25519toCBigNum(n);
    // printf("mod25519: bignumn is %s\n",
    //      bignumn.ToString().c_str());
}

void BigNum256ModMFromBigNum_(bignum256modm bignum, CBigNum number)
{
    memset(bignum, 0, 40);
    CBigNum twopow56(1);
    twopow56 <<= 56;
    for (uint32_t i = 0; i < 5; i++)
    {
        bignum[i] = (number % twopow56).getulong();
        number >>= 56;
    }
}

CBigNum BigNumFromBigNum256ModM_(bignum256modm bignum)
{
    //printf("BigNumFromBigNum256ModM_(): starting with ");
    //print256(bignum);
    CBigNum number(0);
    number += bignum[4] & 0xffffffff;
    for (int32_t i = 3; i >= 0; i--)
    {
        number <<= 56;
        number += bignum[i] & 0xffffffffffffff;
    }
    return number;
}

static const uint8_t ed25519_order_[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};


inline CBigNum F25519ToBigNum_(const uint8_t *x)
{
    CBigNum number(0);
    for (int32_t i = 32 - 1; i >=0 ; i--)
    {
        number <<= 8;
        uint64_t n = x[i];
        number = number + CBigNum(n);
    }
    return number;
}


/*
	Generates a (extsk[0..31]) and aExt (extsk[32..63])
*/

DONNA_INLINE static void
ed25519_extsk(hash_512bits extsk, const ed25519_secret_key sk) {
	ed25519_hash(extsk, sk, 32);
	extsk[0] &= 248;
	extsk[31] &= 127;
	extsk[31] |= 64;
}

static void
ed25519_hram(hash_512bits hram, const ed25519_signature RS, const ed25519_public_key pk, const unsigned char *m, size_t mlen) {
	ed25519_hash_context ctx;
	ed25519_hash_init(&ctx);
	ed25519_hash_update(&ctx, RS, 32);
	ed25519_hash_update(&ctx, pk, 32);
	ed25519_hash_update(&ctx, m, mlen);
	ed25519_hash_final(&ctx, hram);
}

void
ED25519_FN(ed25519_publickey) (const ed25519_secret_key sk, 
                               ed25519_public_key pk) {
	bignum256modm a;
	ge25519 ALIGN(16) A;
	hash_512bits extsk;

	/* A = aB */
	ed25519_extsk(extsk, sk);
	expand256_modm(a, extsk, 32);
	ge25519_scalarmult_base_niels(&A, ge25519_niels_base_multiples, a);
	ge25519_pack(pk, &A);
}

void
ED25519_FN(ed25519_publickey_from_number) (const ed25519_secret_key sk, ed25519_public_key pk) {
	bignum256modm a;
	ge25519 ALIGN(16) A;
    ed25519_secret_key sk_clamped;

    memcpy(sk_clamped, sk, 32);

	/* A = aB */
	expand256_modm(a, sk_clamped, 32);
	ge25519_scalarmult_base_niels(&A, ge25519_niels_base_multiples, a);
	ge25519_pack(pk, &A);
}


void
ED25519_FN(ed25519_sign) (const unsigned char *m, size_t mlen, const ed25519_secret_key sk, const ed25519_public_key pk, ed25519_signature RS) {
	ed25519_hash_context ctx;
	bignum256modm r, S, a;
	ge25519 ALIGN(16) R;
	hash_512bits extsk, hashr, hram;

	ed25519_extsk(extsk, sk);

	/* r = H(aExt[32..64], m) */
	ed25519_hash_init(&ctx);
	ed25519_hash_update(&ctx, extsk + 32, 32);
	ed25519_hash_update(&ctx, m, mlen);
	ed25519_hash_final(&ctx, hashr);
	expand256_modm(r, hashr, 64);

	/* R = rB */
	ge25519_scalarmult_base_niels(&R, ge25519_niels_base_multiples, r);
	ge25519_pack(RS, &R);

	/* S = H(R,A,m).. */
	ed25519_hram(hram, RS, pk, m, mlen);
	expand256_modm(S, hram, 64);

	/* S = H(R,A,m)a */
	expand256_modm(a, extsk, 32);
	mul256_modm(S, S, a);

	/* S = (r + H(R,A,m)a) */
	add256_modm(S, S, r);

	/* S = (r + H(R,A,m)a) mod L */	
	contract256_modm(RS + 32, S);
}

int
ED25519_FN(ed25519_sign_open) (const unsigned char *m, size_t mlen, 
                               const ed25519_public_key pk, 
                               const ed25519_signature RS) {
	ge25519 ALIGN(16) R, A;
	hash_512bits hash;
	bignum256modm hram, S;
	unsigned char checkR[32];

	if ((RS[63] & 224) || !ge25519_unpack_negative_vartime(&A, pk))
		return -1;

	/* hram = H(R,A,m) */
	ed25519_hram(hash, RS, pk, m, mlen);
	expand256_modm(hram, hash, 64);

	/* S */
	expand256_modm(S, RS + 32, 32);

	/* SB - H(R,A,m)A */
	ge25519_double_scalarmult_vartime(&R, &A, hram, S);
	ge25519_pack(checkR, &R);

	/* check that R = SB - H(R,A,m)A */
	return ed25519_verify(RS, checkR, 32) ? 0 : -1;
}

#include "ed25519-donna-batchverify.h"

/*
	Fast Curve25519 basepoint scalar multiplication
*/

void
ED25519_FN(curved25519_scalarmult_basepoint) (curved25519_key pk, 
                                              const curved25519_key e) {
	curved25519_key ec;
	bignum256modm s;
	bignum25519 ALIGN(16) yplusz, zminusy;
	ge25519 ALIGN(16) p;
	size_t i;

	/* clamp */
	for (i = 0; i < 32; i++) ec[i] = e[i];
	ec[0] &= 248;
	ec[31] &= 127;
	ec[31] |= 64;

	expand_raw256_modm(s, ec);

	/* scalar * basepoint */
	ge25519_scalarmult_base_niels(&p, ge25519_niels_base_multiples, s);

	/* u = (y + z) / (z - y) */
	curve25519_add(yplusz, p.y, p.z);
	curve25519_sub(zminusy, p.z, p.y);
	curve25519_recip(zminusy, zminusy);
	curve25519_mul(yplusz, yplusz, zminusy);
	curve25519_contract(pk, yplusz);
}

void morph25519_e2m(bignum25519 x_out, const bignum25519 ey, const bignum25519 ez)
{
    ge25519 ALIGN(16) p;
    bignum25519 ALIGN(16) yplusz, zminusy;

    curve25519_add(yplusz, ey, ez);
    curve25519_sub(zminusy, ez, ey);
    curve25519_recip(zminusy, zminusy);
    curve25519_mul(x_out, yplusz, zminusy);
}

void mx2ey(bignum25519 ey, bignum25519 mx)
{
    bignum25519 ALIGN(16) n, d;
    
    curve25519_add(n, mx, one);
    curve25519_recip(n, n);
    curve25519_sub(d, mx, one);
    curve25519_mul(ey, d, n);
}

void F25519toBigNum25519(bignum25519 x, const uint8_t *d)
{
    memset(x, 0, 40);
    bignum25519 ALIGN(16) pow256, _256, term;
    memset(pow256, 0 , 40);
    memset(_256, 0 , 40);
    pow256[0] = 1;
    _256[0] =256;

    for (uint32_t i = 0; i < 32; i++)
    {
        memset(term, 0 , 40);
        term[0] = d[i];
        curve25519_mul(term, pow256, term);
        curve25519_add(x, x, term);
        curve25519_mul(pow256, pow256, _256);
    }
}

static void print256_(const bignum256modm a)
{
    // printf("%lu ", a[0] & 0xffffffffffffff);
    // printf("+(2**56)**1 * %lu ", a[1] & 0xffffffffffffff);
    // printf("+(2**56)**2 * %lu ", a[2] & 0xffffffffffffff);
    // printf("+(2**56)**3 * %lu ", a[3] & 0xffffffffffffff);
    // printf("+(2**56)**4 * %lu\n", a[4] & 0xffffffff);
}



static uint8_t ey2ex(bignum25519 x, bignum25519 y, int parity)
{
    static const uint8_t d_[F25519_SIZE] = {
        0xa3, 0x78, 0x59, 0x13, 0xca, 0x4d, 0xeb, 0x75,
        0xab, 0xd8, 0x41, 0x41, 0x4d, 0x0a, 0x70, 0x00,
        0x98, 0xe8, 0x79, 0x77, 0x79, 0x40, 0xc7, 0x8c,
        0x73, 0xfe, 0x6f, 0x2b, 0xee, 0x6c, 0x03, 0x52
    };

    
    // printf("ey2ex: ------ y is ");
    // printhex_(y);

    bignum25519 ALIGN(16) a, b, c, d;

    F25519toBigNum25519(d, d_);
    // printf("ey2ex: ------ d is ");
    // printhex_(d);

    /* Compute c = y^2 */
    curve25519_mul(c, y, y);
    // printf("ey2ex: c is "); printhex_(c);
    
    /* Compute b = (1+dy^2)^-1 */
    curve25519_mul(b, c, d);
    // printf("ey2ex: b is "); printhex_(b);
    curve25519_add(a, b, one);
    // printf("ey2ex: a is "); printhex_(a);
    curve25519_recip(b, a);
    // printf("ey2ex: b is "); printhex_(b);
    

    /* Compute a = y^2-1 */
    curve25519_sub(a, c, one);
    // printf("ey2ex: a is "); printhex_(a);

    /* Compute c = a*b = (y^2+1)/(1-dy^2) */
    curve25519_mul(c, a, b);
    // printf("ey2ex: c is "); printhex_(c);

    /* Compute a, b = +/-sqrt(c), if c is square */
    curve25519_sqrt(a, c);
    // printf("ey2ex: a is "); printhex_(a);
    curve25519_neg(b, a);
    // printf("ey2ex: b is "); printhex_(b);
    // printf("parity is %d\n", parity);
    /* Select one of them, based on the parity bit */
    curved25519_key csk_a, csk_c;
    
    curve25519_contract(csk_a, a);

    // printf("csk_a is ");
    // for (uint32_t i = 0; i < 32; i++)
    //     printf("%02x", csk_a[i]);
    // printf("\n");

    if ((parity ^ csk_a[0]) & 1)
        curve25519_copy(x, b);
    else
        curve25519_copy(x, a);
    
    mod25519(x);
    // printf("ey2ex: x is "); printhex_(x);

    /* Verify that x^2 = c */
    curve25519_mul(a, x, x);
    mod25519(a);
    mod25519(c);

    
    curve25519_contract(csk_a, a);
    curve25519_contract(csk_c, c);
    // printf("ey2ex: a is "); printhex_(a);
    // printf("ey2ex: c is "); printhex_(c);

    if (memcmp(csk_a, csk_c, 32))
        printf("ey2ex failed!\n");
    return 1;
}

/* Return a parity bit for the Edwards X coordinate */
int morph25519_eparity(bignum25519 edwards_x)
{
    unsigned char r[32];
    // printf("eparity(): r is ");
    curve25519_contract(r, edwards_x);
    // for (uint8_t i = 0; i < 32; i ++)
    //     printf("%02x", r[i]);
    // printf("\n");
    
    return r[0] & 1;
}

uint8_t morph25519_m2e(bignum25519 ex, bignum25519 ey,
                       bignum25519 mx, int parity)
{
    uint8_t ok;
    
    // printf("morph25519_m2e: mx is ");
    // printhex_(mx);
    mx2ey(ey, mx);
    //mod25519(ey);
    // printf("morph25519_m2e: ey is: ");
    // printhex_(ey);
    ok = ey2ex(ex, ey, parity) + 1;
    mod25519(ex);
    mod25519(ey);
    // printf("morph25519_m2e: ex is ");
    // printhex_(ex);



    return ok;
}


/* Conversion to and from projective coordinates */
void ed25519_project(ge25519 *p, bignum25519 x, bignum25519 y)
{
    curve25519_copy(p->x, x);
    curve25519_copy(p->y, y);
    curve25519_copy(p->z, one);
    curve25519_mul(p->t, x, y);
}

void ed25519_unproject(bignum25519 x, bignum25519 y, const ge25519 *p)
{
    bignum25519 ALIGN(16) z1;

    memset(z1, 0, 40);

    curve25519_recip(z1, p->z);

    curve25519_mul(x, p->x, z1);
    curve25519_mul(y, p->y, z1);
}

void
ED25519_FN(curved25519_scalarmult) (curved25519_key pkout,
                                    int* parity_out,
                                    curved25519_key pkin,
                                    int parity,
                                    const bignum256modm multiplicand) {
    bignum256modm s;
    bignum25519 ALIGN(16) yplusz, zminusy, ex, ey, mx;
    ge25519 ALIGN(16) p, ed25519_element;
    ed25519_public_key epk;
    size_t i;

    curve25519_expand(mx, pkin);
    bignum256modm zero;
    memset(&zero, 0, 40);
        
    morph25519_m2e(ex, ey, mx, parity);

    curve25519_copy(ed25519_element.x, ex);
    curve25519_copy(ed25519_element.y, ey);
    curve25519_copy(ed25519_element.z, one);
    curve25519_mul(ed25519_element.t, ex, ey);
    
    ge25519_double_scalarmult_vartime(&p, &ed25519_element, multiplicand, zero);
    
    morph25519_e2m(s, p.y, p.z);
    curve25519_contract(pkout, s);
    ed25519_unproject(ex, ey, &p);
    *parity_out = morph25519_eparity(ex);
}


void
ED25519_FN(curved25519_add) (curved25519_key pkout,
                             int* parity_out,
                             const curved25519_key pkin1,
                             const int parity1,
                             const curved25519_key pkin2,
                             const int parity2)
{
    bignum25519 ALIGN(16) ex1, ey1, ex2, ey2;
    ge25519 ALIGN(16) p1, p2, p1plusp2;
    bignum25519 ALIGN(16) montgomery_x_in1, montgomery_x_in2, 
                          montgomery_x_out;

    curve25519_expand(montgomery_x_in1, pkin1);
    curve25519_expand(montgomery_x_in2, pkin2);
    // printf("pre-add: mx1 = "); printhex_(montgomery_x_in1);
    // printf("pre-add: mx2 = "); printhex_(montgomery_x_in2);

    // printf("parity1: %d    parity2: %d\n", parity1, parity2);

    morph25519_m2e(ex1, ey1, montgomery_x_in1, parity1);
    morph25519_m2e(ex2, ey2, montgomery_x_in2, parity2);
    // printf("pre-add: ex1 = "); printhex_(ex1);
    // printf("pre-add: ey1 = "); printhex_(ey1);
    // printf("pre-add: ex2 = "); printhex_(ex2);
    // printf("pre-add: ey2 = "); printhex_(ey2);

    ed25519_project(&p1, ex1, ey1);
    ed25519_project(&p2, ex2, ey2);
    ge25519_add(&p1plusp2, &p1, &p2);

    ed25519_unproject(ex1, ey1, &p1plusp2);

    // printf("add: ex1 = ");
    // printhex_(ex1);
    // printf("\n");
    // printf("add: ey1 = ");
    // printhex_(ey1);
    // printf("\n");

    mod25519(ex1);
    *parity_out = morph25519_eparity(ex1);

    ed25519_public_key epk;
    ge25519_pack(epk, &p1plusp2);
    morph25519_e2m(montgomery_x_out, p1plusp2.y, p1plusp2.z);
    mod25519(montgomery_x_out);
    curve25519_contract(pkout, montgomery_x_out);
}

