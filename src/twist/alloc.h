#ifndef TWIST_ALLOC_H
#define TWIST_ALLOC_H
#include "scrypt-jane-twist.h"
#include <stdio.h>
#include <stdlib.h>


inline void scrypt_fatal_error_(const char *s) {
    fprintf(stderr, "%s", s);
}

static scrypt_aligned_alloc
scrypt_alloc(uint64_t size) {
    static const size_t max_alloc = (size_t)-1;
    scrypt_aligned_alloc aa;
#ifndef SCRYPT_BLOCK_BYTES
    int SCRYPT_BLOCK_BYTES = 128;
#endif
    size += (SCRYPT_BLOCK_BYTES - 1);
    if (size > max_alloc)
        scrypt_fatal_error_("scrypt: not enough address space on this CPU to allocate required memory");
    aa.mem = (uint8_t *)malloc((size_t)size);
    aa.ptr = (uint8_t *)(((size_t)aa.mem + (SCRYPT_BLOCK_BYTES - 1)) & ~(SCRYPT_BLOCK_BYTES - 1));
    if (!aa.mem)
        scrypt_fatal_error_("scrypt: out of memory");
    return aa;
}
#endif
