// Copyright (c) 2014 Flex Developers
// Distributed under version 3 of the Gnu Affero GPL software license, see the accompanying
// file COPYING for details
#ifndef FLEX_WORK_H
#define FLEX_WORK_H

#include "crypto/flexcrypto.h"
#include "crypto/uint256.h"
#include "twist/twistcpp.h"

#include "log.h"
#define LOG_CATEGORY "work.h"

#define FLEX_WORK_NUMBER_OF_LINKS 128
#define FLEX_WORK_NUMBER_OF_SEGMENTS 32

uint160 target_from_difficulty(uint160 difficulty);


class TwistWorkCheck;

class TwistWorkProof
{
public:
    TwistWorkProof(uint256 memory_seed, uint64_t memory_factor, uint160 difficulty);

    uint256 memoryseed;
    uint64_t N_factor;
    uint160 target;
    uint160 link_threshold;
    uint160 quick_verifier;
    uint32_t num_links;
    uint32_t num_segments;
    std::vector<uint8_t> seeds;
    std::vector<uint64_t> links;
    std::vector<uint32_t> link_lengths;
    uint160 difficulty_achieved;

    TwistWorkProof();
    TwistWorkProof(uint256 memoryseed, 
                   uint64_t N_factor, 
                   uint160 target, 
                   uint160 link_threshold,
                   uint32_t num_segments);

    uint160 DifficultyAchieved();

    uint64_t Length();

    IMPLEMENT_SERIALIZE(
        READWRITE(memoryseed);
        READWRITE(num_segments);
        READWRITE(N_factor);
        READWRITE(target);
        READWRITE(link_threshold);
        READWRITE(quick_verifier);
        READWRITE(num_links);
        READWRITE(seeds);
        READWRITE(links);
        READWRITE(link_lengths);
    )

    uint256 GetHash() const;

    int DoWork(unsigned char *working);

    uint160 WorkDone();

    TwistWorkCheck CheckRange(uint32_t seed, uint32_t num_segments_to_check,
                              uint32_t link, uint32_t links_to_check);

    TwistWorkCheck SpotCheck();

    uint64_t MegabytesUsed()
    {
        int64_t power_of_two = N_factor - 7;
        if (power_of_two < 0)
            return 0;

        uint64_t number_of_megabytes = 1;
        while (power_of_two > 0)
        {
            power_of_two /= 2;
            number_of_megabytes *= 2;
        }
        return number_of_megabytes;
    }
};


class TwistWorkCheck
{
public:
    uint256 proof_hash;
    uint32_t start_link;
    uint32_t end_link;
    uint8_t valid;
    uint32_t failure_step;
    uint32_t failure_link;
    uint32_t failure_seed;

    TwistWorkCheck();

    TwistWorkCheck(TwistWorkProof &proof);

    IMPLEMENT_SERIALIZE(
        READWRITE(proof_hash);
        READWRITE(start_link);
        READWRITE(end_link);
        READWRITE(valid);
        READWRITE(failure_link);
        READWRITE(failure_step);
        READWRITE(failure_seed);
    )

    uint256 GetHash();

    uint8_t Valid()
    {
        return valid;
    }

    int VerifyInvalid(TwistWorkProof &proof);
};

#endif
