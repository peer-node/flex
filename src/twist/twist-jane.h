#ifndef SCRYPT_JANE_H
#define SCRYPT_JANE_H

/*
    Nfactor: Increases CPU & Memory Hardness
    N = (1 << (Nfactor + 1)): How many times to mix a chunk and how many temporary chunks are used

    rfactor: Increases Memory Hardness
    r = (1 << rfactor): How large a chunk is

    A block is the basic mixing unit (salsa/chacha block = 64 bytes)
    A chunk is (2 * r) blocks

    ~Memory used = (N + 2) * ((2 * r) * block size)
*/

typedef __uint128_t uint128_t;

#include <stdlib.h>
#include <stdint.h>

typedef struct scrypt_aligned_alloc_t {
    uint8_t *mem, *ptr;
} scrypt_aligned_alloc;


typedef void (*scrypt_fatal_errorfn)(const char *msg);

static
void scrypt_set_fatal_error(scrypt_fatal_errorfn fn);

static
void scrypt(const unsigned char *password, 
            size_t password_len, 
            const unsigned char *salt, 
            size_t salt_len, 
            unsigned char Nfactor, 
            unsigned char rfactor, 
            unsigned char pfactor, 
            unsigned char *out, 
            size_t bytes);

static
void twist_prepare(const uint8_t *password, 
                   size_t password_len, 
                   const uint8_t *salt, 
                   size_t salt_len, 
                   uint8_t Nfactor, 
                   uint8_t rfactor, 
                   uint32_t numSegments, 
                   scrypt_aligned_alloc *V, 
                   scrypt_aligned_alloc *VSeeds);

static
void twist_work(uint8_t *pFirstLink,
                uint8_t Nfactor, 
                uint8_t rfactor, 
                uint32_t numSegments, 
                scrypt_aligned_alloc *V, 
                uint64_t *Links, 
                uint32_t *linkLengths, 
                uint32_t *numLinks, 
                uint32_t W, 
                uint128_t T, 
                uint128_t Target, 
                uint128_t *quick_verifier, 
                uint8_t *pfWorking);

static
uint32_t twist_verify(uint8_t *pHashInput,
                      uint8_t Nfactor, 
                      uint8_t rfactor, 
                      uint32_t numSegments, 
                      scrypt_aligned_alloc *VSeeds, 
                      uint32_t startSeed, 
                      uint32_t numSeedsToUse, 
                      uint64_t *Links, 
                      uint32_t *linkLengths, 
                      uint32_t *numLinks, 
                      uint32_t startCheckLink, 
                      uint32_t endCheckLink, 
                      uint128_t T, 
                      uint128_t Target, 
                      uint32_t *workStep, 
                      uint32_t *link, 
                      uint32_t *seed);

static
uint128_t twist_quickcheck(uint8_t *pHashInput,
                           uint8_t Nfactor, 
                           uint8_t rfactor, 
                           uint32_t numSegments, 
                           scrypt_aligned_alloc *VSeeds, 
                           uint64_t *Links, 
                           uint32_t *linkLengths, 
                           uint32_t *numLinks, 
                           uint128_t T, 
                           uint128_t Target,
                           uint128_t *quick_verifier);
 
static void
scrypt_free(scrypt_aligned_alloc *aa) {
    free(aa->mem);
}

static scrypt_aligned_alloc
scrypt_alloc(uint64_t size);

#endif /* SCRYPT_JANE_H */
