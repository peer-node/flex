/*
 * Copyright 2014, Peer Node
 * Gnu Affero GPLv3 License
 *
 * The twist proof of work was developed for the Flex exchange
 */ 

#include <stdio.h>
#include <stdint.h>
#include <numeric>
#include <sys/time.h>
#include "twistcpp.h"

using namespace std;

typedef __uint128_t uint128_t;

struct timeval last_, now_;

void saytime_() 
{
    last_ = now_; gettimeofday(&now_, NULL);
    printf("%lf seconds\n", 
           now_.tv_sec - last_.tv_sec + (now_.tv_usec - last_.tv_usec) * 1e-6);
}

uint128_t MeasureDifficulty(uint128_t nWorkDoneResult)
{
    if (nWorkDoneResult == 0)
    {
        printf("MeasureDifficulty(): quickcheckresult was 0!\n");
        return 0;
    }
    uint128_t big = 1;
    big = big << 127;
    return big / (nWorkDoneResult / 2);
}

int main(void) 
{
    uint32_t Nfactor=17,      // Memory usage parameter
             rfactor=4,       // Block size parameter
             numSegments=64;  // Number of independently-generatable segments

    uint8_t *pHashInput = (unsigned char *)"01234567890123456789012345678904";
    uint64_t nFirstLink = 1234567812345678ULL;

    vector<uint8_t> vnSeeds;
    vector<uint64_t> vnLinks;
    vector<uint32_t> vnLinkLengths;

    uint128_t LinkThreshold = 1; LinkThreshold <<= 118;
    uint128_t Target = 1; Target <<= 113;
    uint128_t quick_verifier;

    uint8_t  fWorking = 1;

    uint32_t failureWorkStep, failureLink, failureSeed;

    printf("starting work...\n");
    gettimeofday(&now_, NULL);
    twist_dowork(pHashInput, (uint8_t *)&nFirstLink,
                 Target, LinkThreshold, &quick_verifier,
                 Nfactor, rfactor, numSegments,
                 vnSeeds, vnLinks, vnLinkLengths, &fWorking);
    saytime_();

    /* quick verification */
    printf("Quick verification result: difficulty achieved = %llu\n",
        MeasureDifficulty(
            twist_doquickcheck(pHashInput,
                Target, LinkThreshold, &quick_verifier,
                Nfactor, rfactor, numSegments,
                vnSeeds, vnLinks, vnLinkLengths)));
    saytime_();
    
    /* spot checks */
    uint32_t numLinks = vnLinks.size();
    for (uint32_t i = 0; i < 10; i++)
    {
        /* check pseudo-randomly chosen links / memory segments */
        uint32_t spotCheckStartSeed = ((i + 17) * 1236571) % (numSegments - 1);
        uint32_t spotCheckStartLink = ((i + 19) * 1236573) % (numLinks - 1);
        printf("spot check %d result: %u\n", i + 1,
            twist_doverify(pHashInput,
                           Target, LinkThreshold, Nfactor, rfactor, 
                           numSegments, vnSeeds, vnLinks, vnLinkLengths,
                           spotCheckStartSeed, 1, 
                           spotCheckStartLink, spotCheckStartLink + 1, 
                           &failureWorkStep, &failureLink, &failureSeed));
        saytime_();
    }

    /* full verification */
    printf("Full Verification result: %u\n",
        twist_doverify(pHashInput, Target, LinkThreshold, Nfactor, rfactor, 
                       numSegments, vnSeeds, vnLinks, vnLinkLengths,
                       0, numSegments, 0, numLinks,
                       &failureWorkStep, &failureLink, &failureSeed));
    saytime_();

	return 0;
}
