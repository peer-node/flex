/*
 * Copyright 2015, Peer Node
 * Gnu Affero GPLv3 License
 *
 * The twist proof of work was developed for Flex
 *
 * This project contains code previously released under the MIT License
 * by floodyberry.
 */

#ifndef TWISTCPP_H
#define TWISTCPP_H

#include "twist.h"
#include "scrypt-jane-twist.h"
#include <vector>


inline int 
twist_dowork(uint8_t *pHashInput,
             uint8_t *pFirstLink,
             uint128_t target,
             uint128_t link_threshold,
             uint128_t *quick_verifier,
             uint8_t Nfactor,
             uint8_t rfactor,
             uint32_t numSegments,
             std::vector<uint8_t> &vnSeeds,
             std::vector<uint64_t> &vnLinks,
             std::vector<uint32_t> &vnLinkLengths,
             unsigned char *pfWorking) 
{
    scrypt_aligned_alloc *V, *VSeeds;
    uint32_t numLinks = 0;
    uint32_t max_worksteps = (uint32_t) (10 * ((((uint128_t)1ULL) << 127) / target));
    int result;

    V = (scrypt_aligned_alloc *)malloc(sizeof(scrypt_aligned_alloc));
    VSeeds =(scrypt_aligned_alloc *)malloc(sizeof(scrypt_aligned_alloc));

    vnLinks.resize(MAX_LINKS);
    vnLinkLengths.resize(MAX_LINKS);

    twist_prepare((const uint8_t *)pHashInput, 32, (const uint8_t *)"", 0, 
                  Nfactor, rfactor, numSegments, V, VSeeds);
    twist_work(pFirstLink, Nfactor, rfactor, numSegments, V, 
               &vnLinks[0], &vnLinkLengths[0], 
               &numLinks, max_worksteps, link_threshold, target, 
               quick_verifier, pfWorking);

    scrypt_free(V);
    free(V);
    result = *quick_verifier != 0;
    
    vnSeeds.resize(numSegments);
    memcpy(&vnSeeds[0], VSeeds->ptr, numSegments);
    vnLinks.resize(numLinks);
    vnLinkLengths.resize(numLinks);
    printf("numlinks is %u\n", numLinks);
    if (result)
    {    
        printf("proof ok\n");
        *pfWorking = 1;
    }
    
    scrypt_free(VSeeds);
    free(VSeeds);
    return result;
}

inline uint128_t
twist_doquickcheck(unsigned char *pHashInput,
                   uint128_t target,
                   uint128_t link_threshold,
                   uint128_t *quick_verifier,
                   uint8_t Nfactor,
                   uint8_t rfactor,
                   uint32_t numSegments,
                   std::vector<uint8_t> vnSeeds,
                   std::vector<uint64_t> vnLinks,
                   std::vector<uint32_t> vnLinkLengths)
{
    scrypt_aligned_alloc *VSeeds;
    uint64_t *Links = &vnLinks[0];
    uint32_t *linkLengths = &vnLinkLengths[0];
    uint32_t numLinks = (uint32_t) vnLinks.size();
    uint128_t result = 0;

    VSeeds =(scrypt_aligned_alloc *)malloc(sizeof(scrypt_aligned_alloc));
    (*VSeeds).ptr = (uint8_t *)&vnSeeds[0];
    result = twist_quickcheck(pHashInput, Nfactor, rfactor, numSegments, 
                              VSeeds, Links, linkLengths,
                              &numLinks, link_threshold, target,
                              quick_verifier);
    free(VSeeds);
    return result;
}

inline
int twist_doverify(uint8_t *pHashInput,
                   uint128_t target,
                   uint128_t link_threshold,
                   uint8_t Nfactor,
                   uint8_t rfactor,
                   uint32_t numSegments,
                   std::vector<uint8_t> vnSeeds,
                   std::vector<uint64_t> vnLinks,
                   std::vector<uint32_t> vnLinkLengths,
                   uint32_t startSeed,
                   uint32_t numSeedsToUse,
                   uint32_t startCheckLink,
                   uint32_t endCheckLink,
                   uint32_t *workStep,
                   uint32_t *link,
                   uint32_t *seed)
{
    scrypt_aligned_alloc *VSeeds;
    uint64_t *Links = &vnLinks[0];
    uint32_t *linkLengths = &vnLinkLengths[0];
    uint32_t numLinks = (uint32_t) vnLinks.size();
    int result;

    VSeeds =(scrypt_aligned_alloc *)malloc(sizeof(scrypt_aligned_alloc));
    (*VSeeds).ptr = (uint8_t *)&vnSeeds[0];
    result = twist_verify(pHashInput, Nfactor, rfactor, numSegments, 
                          VSeeds, startSeed, numSeedsToUse, 
                          Links, linkLengths, &numLinks, 
                          startCheckLink, endCheckLink, 
                          link_threshold, target,
                          workStep, link, seed);
    free(VSeeds);
    return result;
}

#endif
