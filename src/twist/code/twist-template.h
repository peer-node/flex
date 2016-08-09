#ifndef __TOKEN__
#define __TOKEN__




#if !defined(SCRYPT_CHOOSE_COMPILETIME) || !defined(SCRYPT_HAVE_ROMIX)

#if defined(SCRYPT_CHOOSE_COMPILETIME)
#undef SCRYPT_ROMIX_FN
#define SCRYPT_ROMIX_FN scrypt_ROMix
#endif

#undef SCRYPT_HAVE_ROMIX
#define SCRYPT_HAVE_ROMIX

#ifndef SCRYPT_MIX_FN
#define SCRYPT_MIX_FN salsa64_core_basic
#endif

#if !defined(SCRYPT_CHUNKMIX_FN)

#define SCRYPT_CHUNKMIX_FN scrypt_ChunkMix_basic

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "../twistcpp.h"

/*
	Bout = ChunkMix(Bin)

	2*r: number of blocks in the chunk
*/
static void asm_calling_convention
SCRYPT_CHUNKMIX_FN(scrypt_mix_word_t *Bout/*[chunkWords]*/,
                   scrypt_mix_word_t *Bin/*[chunkWords]*/,
                   scrypt_mix_word_t *Bxor/*[chunkWords]*/,
                   uint32_t r)
{
#if (defined(X86ASM_AVX2) || defined(X86_64ASM_AVX2) || defined(X86_INTRINSIC_AVX2))
	scrypt_mix_word_t ALIGN22(32) X[SCRYPT_BLOCK_WORDS], *block;
#else
	scrypt_mix_word_t ALIGN22(16) X[SCRYPT_BLOCK_WORDS], *block;
#endif
	uint32_t i, j, blocksPerChunk = r * 2, half = 0;


	/* 1: X = B_{2r - 1} */
	block = scrypt_block(Bin, blocksPerChunk - 1);
	for (i = 0; i < SCRYPT_BLOCK_WORDS; i++)
		X[i] = block[i];

	if (Bxor) {
		block = scrypt_block(Bxor, blocksPerChunk - 1);
		for (i = 0; i < SCRYPT_BLOCK_WORDS; i++)
			X[i] ^= block[i];
	}

	/* 2: for i = 0 to 2r - 1 do */
	for (i = 0; i < blocksPerChunk; i++, half ^= r) {
		/* 3: X = H(X ^ B_i) */
		block = scrypt_block(Bin, i);
		for (j = 0; j < SCRYPT_BLOCK_WORDS; j++)
			X[j] ^= block[j];

		if (Bxor) {
			block = scrypt_block(Bxor, i);
			for (j = 0; j < SCRYPT_BLOCK_WORDS; j++)
				X[j] ^= block[j];
		}
		SCRYPT_MIX_FN(X);

		/* 4: Y_i = X */
		/* 6: B'[0..r-1] = Y_even */
		/* 6: B'[r..2r-1] = Y_odd */
		block = scrypt_block(Bout, (i / 2) + half);
		for (j = 0; j < SCRYPT_BLOCK_WORDS; j++)
			block[j] = X[j];
	}
}
#endif

/*
	X = ROMix(X)

	X: chunk to mix
	Y: scratch chunk
	N: number of rounds
	V[N]: array of chunks to randomly index in to
	2*r: number of blocks in a chunk
*/


static void NOINLINE FASTCALL
SCRYPT_ROMIX_FN(scrypt_mix_word_t *X/*[chunkWords]*/, 
                scrypt_mix_word_t *Y/*[chunkWords]*/, 
                scrypt_mix_word_t *V/*[N * chunkWords]*/, 
                uint32_t N, uint32_t r) 
{
	  uint32_t i, j, chunkWords = (uint32_t)(SCRYPT_BLOCK_WORDS * r * 2);
	  scrypt_mix_word_t *block = V;

	  SCRYPT_ROMIX_TANGLE_FN(X, r * 2);

  	/* 1: X = B */
  	/* implicit */

	  /* 2: for i = 0 to N - 1 do */
	  memcpy(block, X, chunkWords * sizeof(scrypt_mix_word_t));
	  for (i = 0; i < N - 1; i++, block += chunkWords) {
    		/* 3: V_i = X */
    		/* 4: X = H(X) */
		    SCRYPT_CHUNKMIX_FN(block + chunkWords, block, NULL, r);
	  }
	  SCRYPT_CHUNKMIX_FN(X, block, NULL, r);

	  /* 6: for i = 0 to N - 1 do */
	  for (i = 0; i < N; i += 2) 
    {
    		/* 7: j = Integerify(X) % N */
    		j = (uint32_t) (X[chunkWords - SCRYPT_BLOCK_WORDS] & (N - 1));

    		/* 8: X = H(Y ^ V_j) */
    		SCRYPT_CHUNKMIX_FN(Y, X, scrypt_item(V, j, chunkWords), r);

    		/* 7: j = Integerify(Y) % N */
    		j = (uint32_t) (Y[chunkWords - SCRYPT_BLOCK_WORDS] & (N - 1));

		    /* 8: X = H(Y ^ V_j) */
		    SCRYPT_CHUNKMIX_FN(X, Y, scrypt_item(V, j, chunkWords), r);
	  }

	  /* 10: B' = X */
	  /* implicit */

	  SCRYPT_ROMIX_UNTANGLE_FN(X, r * 2);
}

/*
    TWIST_prepare():

    Generate data V from X, broken into numSegments segments, each of
    which can be regenerated from a seed. Data from many earlier parts of
    a segment, not just the preceding block, are used to generate the
    next block in that segment. This prevents low-memory attackers
    from, for example, storing every odd block and regenerating the 
    even blocks on the fly in one hash as information from them is needed.

    Part of each segment's first block is set equal to part
    of X. This allows quick verification during link chain checking
    that the segments are not being reused for different X's, and
    also allows the use of much smaller seeds (16 bytes vs 4096 bytes).

	X: chunk to mix
	N: number of rounds
	V[N]: array of chunks to generate
    VSeeds[numSegments]: Seeds to generate segments of V
	2*r: number of blocks in a chunk
*/

#ifndef TWIST_PREPARE_BLOCK_FN_
#define TWIST_PREPARE_BLOCK_FN_

static void NOINLINE FASTCALL
TWIST_PREPARE_BLOCK_FN(scrypt_mix_word_t *X,
                       scrypt_mix_word_t *block,
                       scrypt_mix_word_t *V,
                       uint8_t *VSeeds,
                       uint32_t startSeed,
                       uint32_t *numSeeds,
                       uint32_t N, 
                       uint32_t r,
                       uint32_t numSegments,
                       uint32_t chunkWords,
                       uint32_t i) 
{
    uint32_t k, segmentStart, segmentLocation, segmentSize = N / numSegments;
    uint32_t mixer;

    /* 3: Use earlier data from segment, if any, to alter V_i */
    segmentLocation = i % segmentSize;
    if (segmentLocation > 0) 
    {
        segmentStart = i - segmentLocation;
        for (k = 0; k < chunkWords; k++) 
        {
            mixer = (uint32_t)block[k] % segmentLocation * chunkWords;
            block[k] ^= V[segmentStart * chunkWords + mixer];
        }
    }
    else 
    {
        memcpy(&VSeeds[(*numSeeds)], block, 1);
        for (i = 0; i < chunkWords; i++)
            block[i] = X[i] * (1 + (*numSeeds)) - i;
        memcpy(block, &VSeeds[(*numSeeds)], 1);
        (*numSeeds)++;
    }
    /* 4: V_(i+1) = H(V_i) */
    SCRYPT_CHUNKMIX_FN(block + chunkWords, block, NULL, r);
}

/* populate V with specified segments of data using the given seeds */
static void NOINLINE FASTCALL
TWIST_PREPARESEGMENTS_FN(
        scrypt_mix_word_t *X,
        scrypt_mix_word_t *V,
        uint8_t *VSeeds,
        uint32_t startSeed,
        uint32_t numSeedsToUse,
        uint32_t chunkWords,
        uint32_t segmentSize,
        uint32_t *numSeeds,
        uint32_t N, 
        uint32_t r,
        uint32_t numSegments)
{
    uint32_t i;
	scrypt_mix_word_t *block = V;

	memcpy(block, &VSeeds[startSeed], 1);

	for (i = 0; i < numSeedsToUse * segmentSize; i++, block += chunkWords) 
        TWIST_PREPARE_BLOCK_FN(X, block, V, VSeeds, startSeed,
                               numSeeds, N, r, numSegments, chunkWords, i);
}

static void NOINLINE FASTCALL
TWIST_PREPARE_FN(scrypt_mix_word_t *X, 
                 scrypt_mix_word_t *V,
                 uint8_t *VSeeds,
                 uint32_t *numSeeds,
                 uint32_t N, 
                 uint32_t r,
                 uint32_t numSegments) 
{
    uint32_t chunkWords = (uint32_t)(SCRYPT_BLOCK_WORDS * r * 2);
    *numSeeds = 0;
	
    SCRYPT_ROMIX_TANGLE_FN(X, r * 2);
    memcpy(VSeeds, X, 1);
    TWIST_PREPARESEGMENTS_FN(X, V, VSeeds, 0, numSegments, 
                             chunkWords, N / numSegments, numSeeds, 
                             N, r, numSegments);
}

/*
    TWIST_work(): 
    
    Generates chain of links as follows:

    1. X is hashed (X = hash(X)) and combined with the some of the
       prepared data V located at Integerify(X) % len(V).

    2. If match is sufficiently good for a new link 
       or max link length has been reached, X is set equal to 
       that part of V and added to the list of links.

    3. If match is sufficiently good to stop working,
       stop; otherwise, goto 1.

    The resulting chain of links proves that all of V was accessible
    during the work process.

	X: input variable
	Y: variable
	N: number of rounds which were used to prepare data V
	2*r: number of blocks in a prepared chunk
	V[N]: prepared data
    links[]: list of links in chain
    link_lengths: number of steps/hashes from one link to the next
    link_threshold: Threshold for new link
    current_link_length: length of current link
    target: Threshold for stopping work, if any
    keep_working: external signal to keep / stop working
*/

#define MIXESPERSTEP 2
#define PADBYTE 85   /* = 01010101, padding for X when new link is found */
#define MAXLINKLENGTHFACTOR 8  /* max link length = 8 * expected length */
#include <stdio.h>
#include <stdlib.h>

static uint32_t MaxLinkLength(uint128_t T)
{
    uint128_t numerator = 1;
    numerator = numerator << 127;
    return (uint32_t) (numerator / (T / (2 * MAXLINKLENGTHFACTOR)));
}

static void NOINLINE FASTCALL
TWIST_WORK_FN(scrypt_mix_word_t *X_in, 
              scrypt_mix_word_t *Y,
              scrypt_mix_word_t *V,
              uint32_t memory_size_in_chunks,
              uint32_t r, 
              uint32_t number_of_segments,
              uint64_t *links,
              uint32_t *link_lengths,
              uint32_t *number_of_links,
              uint32_t minimum_number_of_links,
              uint128_t link_threshold,
              uint32_t *current_link_length,
              uint128_t target,
              uint128_t *quick_verifier,
              unsigned char *keep_working)
{
	uint32_t i, j, k; 
    uint32_t words_per_chunk = (uint32_t)(SCRYPT_BLOCK_WORDS * r * 2);
    uint32_t max_link_length = MaxLinkLength(link_threshold);

    scrypt_mix_word_t *X = (scrypt_mix_word_t *) malloc(SCRYPT_BLOCK_WORDS * sizeof(scrypt_mix_word_t));

    memcpy(X, X_in, 8);
    X[1] = 0;
    memset(&X[2], PADBYTE, SCRYPT_BLOCK_WORDS * sizeof(scrypt_mix_word_t) - 16);
    links[0] = *((uint64_t *)X);

    *number_of_links = 1;

	for (i = 0; true; i += 1)
    {    
        if (not (*keep_working))
        {
            *quick_verifier = 0;
            break;
        }
		/* X = H^MIXESPERSTEP(X) */
        
        for (k = 0; k < MIXESPERSTEP; k++)
        {
    		SCRYPT_MIX_FN(X); 
        }
        (*current_link_length)++;

		/* j = Integerify(X) % Number of uint64_t's in V */
		j = (uint32_t) (X[1] & (memory_size_in_chunks * words_per_chunk - 1));

        j = j - (j % 2);
        
        /* if H(X[0]^V_j, V_(j+1), linklength) <= target: stop;
         * if < link_threshold: X = V_j, new link */
        Y[0] = X[0] ^ V[j];
        Y[1] = V[j + 1];
        Y[2] = *current_link_length; // concatenate link length to prevent cycles within link

        memset(&Y[3], PADBYTE, SCRYPT_BLOCK_WORDS * sizeof(scrypt_mix_word_t) - 24);
        SCRYPT_MIX_FN(Y);

        if (*((uint128_t *)Y) <= target and *number_of_links + 2 > minimum_number_of_links)
        {
            *keep_working = 0;
            link_lengths[*number_of_links - 1] = *current_link_length;
            links[*number_of_links] = *((uint64_t *)&V[j]);
            (*number_of_links)++;
            memcpy(quick_verifier, X, 16);
            free(X);
            return;
        }

        if (*((uint128_t *)Y) < link_threshold || *current_link_length == max_link_length)
        {
            link_lengths[*number_of_links - 1] = *current_link_length;

            memcpy(X, &V[j], 8);
            X[1] = *number_of_links; // concatenate link and link number to prevent link cycles
            memset(&X[2], PADBYTE, SCRYPT_BLOCK_WORDS * sizeof(scrypt_mix_word_t) - 16);

            links[*number_of_links] = X[0];

            (*number_of_links)++;

            *current_link_length = 0;
        }
	}
    *quick_verifier = 0;
    free(X);
}

/*
    TWIST_verify(): 
    
    Verify that part of a chain of links is consistent with part
    of the prepared data, V.

	X: variable
	Y: X xor (part of V)
	N: number of rounds which were used to prepare data V
	2*r: number of blocks in a prepared chunk
	V[]: prepared data
    VSeeds: seeds to generate segments of V
    startSeed: seed for first segment of V to be recreated and checked
    numSeedsToUse: number of seeds to use to regenerate segments to check
    Links[]: list of links in chain
    linkLengths: number of steps/hashes from one link to the next
    startCheckLink, endCheckLink: range of links to check
    T: Threshold for new link
    currentLinkLength: length of current link
    Target: Threshold for stopping work, if any
    link, workStep, j: current link, step and data point being checked

*/

static uint32_t NOINLINE FASTCALL
TWIST_VERIFY_FN(
        scrypt_mix_word_t *X, 
        scrypt_mix_word_t *Y,
        scrypt_mix_word_t *V,
        uint8_t *seeds,
        uint32_t start_seed,
        uint32_t number_of_seeds_to_use,
        uint32_t memory_size_in_chunks,
        uint32_t r, 
        uint32_t number_of_segments,
        uint64_t *links,
        uint32_t *link_lengths,
        uint32_t *number_of_links,
        uint32_t first_link_to_check,
        uint32_t last_link_to_check,
        uint128_t link_threshold,
        uint128_t target,
        uint32_t *work_step,
        uint32_t *link,
        uint32_t *j,
        uint32_t minimum_number_of_links)
{
	uint32_t k, start_of_data_to_check, end_of_data_to_check, data_location;
    uint32_t words_per_chunk = (uint32_t)(SCRYPT_BLOCK_WORDS * r * 2);
    uint32_t segment_size = memory_size_in_chunks / number_of_segments;
    uint32_t number_of_seeds;
    uint32_t max_link_length = MaxLinkLength(link_threshold);

    scrypt_mix_word_t *X_original;
    X_original = (scrypt_mix_word_t *)malloc(words_per_chunk * sizeof(scrypt_mix_word_t));
    memcpy(X_original, X, words_per_chunk * sizeof(scrypt_mix_word_t));

    *work_step = 0;

    /* 1: Check link lengths within bounds */
    for (*link = first_link_to_check; *link < last_link_to_check; (*link)++)
        if (link_lengths[*link] > max_link_length)
            return 0;

    if (*number_of_links < minimum_number_of_links)
        return 0;

    SCRYPT_ROMIX_TANGLE_FN(X_original, r * 2);

    /* 2: Generate segments of data V */
    number_of_seeds = start_seed;
    TWIST_PREPARESEGMENTS_FN(X_original, V, seeds, start_seed, number_of_seeds_to_use,
                             words_per_chunk, segment_size, &number_of_seeds, memory_size_in_chunks, r,
                             number_of_segments);

    free(X_original);

    start_of_data_to_check = start_seed * segment_size * words_per_chunk;
    end_of_data_to_check = (start_seed + number_of_seeds_to_use) * segment_size * words_per_chunk;
    
    /* 3: Check each link in the specified link range */
    for (*link = first_link_to_check; *link < last_link_to_check; (*link)++)
    {
        /* 4: Seed X with start of link */
        memcpy(X, &links[*link], 8);
        X[1] = *link;
        memset(&X[2], PADBYTE, SCRYPT_BLOCK_WORDS * sizeof(scrypt_mix_word_t) - 16);

        for (*work_step = 1; *work_step <= link_lengths[*link]; (*work_step)++)
        {
            /* 5: X = H^MIXESPERSTEP(X) */
            for (k = 0; k < MIXESPERSTEP; k++)
                SCRYPT_MIX_FN(X);

            /* 6: j = Integerify(X) % Number of uint64_t's in V */
            *j = (uint32_t) (X[1] & (memory_size_in_chunks * words_per_chunk - 1));
            *j = *j - (*j % 2);

            data_location = *j;

            /* 7: if j is in the data range we are checking, check the rule:
             *    if H(X[0]^V_j,V_(j+1), work step) <= target, stop; if < link threshold, X = V_j */

            if (data_location >= start_of_data_to_check and data_location < end_of_data_to_check)
            {
                Y[0] = X[0] ^ V[data_location - start_of_data_to_check];
                Y[1] = V[data_location + 1 - start_of_data_to_check];
                Y[2] = *work_step;

                memset(&Y[3], PADBYTE, SCRYPT_BLOCK_WORDS * sizeof(scrypt_mix_word_t) - 24);
                SCRYPT_MIX_FN(Y);

                /* after minimum number of links has been reached, should not reach target
                 * until last link */
                uint128_t value_obtained = *((uint128_t *)Y);
                if (*link + 2 >= minimum_number_of_links)
                    if (value_obtained < target and *link != *number_of_links - 2)
                        return 0;
                
                /* should reach link threshold or max length at end of link */
                if (*work_step == link_lengths[*link])
                {
                    if (*((uint128_t *)Y) > link_threshold && *work_step < max_link_length)
                    {
                        return 0; 
                    }

                    /* should reach target, if any, at end of chain */
                    if (*link == *number_of_links - 2)
                    {
                        if (target > 0 && *((uint128_t *)Y) > target)
                        {
                            return 0;
                        }
                    }
                    /* prepare X for next link */
                    memcpy(X, &V[data_location - start_of_data_to_check], 8);
                    X[1] = *link + 1;

                    /* check X matches given link */
                    if (*((uint64_t *)X) != *((uint64_t *)&links[*link + 1]))
                    {
                        return 0;
                    }
                }
                else /* should not reach threshold before end of link */
                    if (*((uint128_t *)Y) <= link_threshold)
                    {
                        return 0;
                    }
            }
        }
    }
    return 1;
}


static uint128_t NOINLINE FASTCALL
TWIST_QUICKCHECK_FN(
        scrypt_mix_word_t *X,
        scrypt_mix_word_t *Y,
        scrypt_mix_word_t *V,
        uint8_t *seeds,
        uint32_t memory_size_in_chunks,
        uint32_t r, 
        uint32_t number_of_segments,
        uint64_t *links,
        uint32_t *link_lengths,
        uint32_t *num_links,
        uint128_t link_threshold,
        uint128_t target,
        uint128_t *quick_verifier,
        uint32_t minimum_number_of_links)
{
	uint32_t i, j, startCheckData; 
    uint32_t words_per_chunk = (uint32_t)(SCRYPT_BLOCK_WORDS * r * 2);
	scrypt_mix_word_t *block = V;
    uint32_t segment_size = memory_size_in_chunks / number_of_segments;
    uint32_t number_of_seeds, start_seed;
    uint32_t max_link_length = MaxLinkLength(link_threshold);

    if (*num_links < minimum_number_of_links)
        return 0;

    scrypt_mix_word_t *X_original;
    X_original = (scrypt_mix_word_t *)malloc(words_per_chunk * sizeof(scrypt_mix_word_t));
    memcpy(X_original, X, words_per_chunk * sizeof(scrypt_mix_word_t));

    for (uint32_t link = 0; link < *num_links; (link)++)
        if (link_lengths[link] > max_link_length)
        {
            return 0;
        }

    memcpy(X, quick_verifier, 16);
    j = (uint32_t) (X[1] & (memory_size_in_chunks * words_per_chunk - 1));

    j = j - (j % 2);

    startCheckData = j - (j % (segment_size * words_per_chunk));
    start_seed = startCheckData / (segment_size * words_per_chunk);

	memcpy(block, &seeds[start_seed], 1);
    number_of_seeds = start_seed;
    
    SCRYPT_ROMIX_TANGLE_FN(X_original, r * 2);
    
	for (i = 0; i < segment_size; i++, block += words_per_chunk)
        TWIST_PREPARE_BLOCK_FN(X_original, block, V, seeds, start_seed,
                               &number_of_seeds, memory_size_in_chunks, r, number_of_segments, words_per_chunk, i);

    free(X_original);

    Y[0] = X[0] ^ V[j - startCheckData];
    Y[1] = V[j + 1 - startCheckData];
    Y[2] = link_lengths[(*num_links) - 2];

    memset(&Y[3], PADBYTE, SCRYPT_BLOCK_WORDS * sizeof(scrypt_mix_word_t) - 24);
    SCRYPT_MIX_FN(Y);

    if (target > 0 && *((uint128_t *)Y) > target)
    {
        return 0;
    }
    return *((uint128_t *)Y);
}

#endif

#endif /* !define(TWIST_PREPARE_FN) */

#endif /* !defined(SCRYPT_CHOOSE_COMPILETIME) || !defined(SCRYPT_HAVE_ROMIX) */

#undef SCRYPT_CHUNKMIX_FN
#undef SCRYPT_ROMIX_FN
#undef TWIST_PREPARE_FN
#undef TWIST_WORK_FN
#undef TWIST_VERIFY_FN
#undef TWIST_QUICKCHECK_FN
#undef SCRYPT_MIX_FN
#undef SCRYPT_ROMIX_TANGLE_FN
#undef SCRYPT_ROMIX_UNTANGLE_FN
#undef __TOKEN__
