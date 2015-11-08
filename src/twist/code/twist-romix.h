#if defined(SCRYPT_SALSA64)
#include "twist-salsa64.h"
#else
	#define SCRYPT_MIX_BASE "ERROR"
	typedef uint32_t scrypt_mix_word_t;
	#define SCRYPT_WORDTO8_LE U32TO8_LE
	#define SCRYPT_WORD_ENDIAN_SWAP U32_SWAP
	#define SCRYPT_BLOCK_BYTES 64
	#define SCRYPT_BLOCK_WORDS (SCRYPT_BLOCK_BYTES / sizeof(scrypt_mix_word_t))
	#if !defined(SCRYPT_CHOOSE_COMPILETIME)
		static void FASTCALL twist_ROMix_prepare_error(
                scrypt_mix_word_t *X/*[chunkWords]*/, 
                scrypt_mix_word_t *V/*[chunkWords * N]*/, 
                uint8_t *VSeeds, 
                uint32_t *numSeeds, 
                uint32_t N, 
                uint32_t r,
                uint32_t numSegments) {}
		static twist_ROMixfn_prepare twist_getROMix_prepare(void) { 
            return twist_ROMix_prepare_error; }
		static void FASTCALL twist_ROMix_work_error(
                scrypt_mix_word_t *X/*[chunkWords]*/, 
                scrypt_mix_word_t *Y, 
                scrypt_mix_word_t *V/*[chunkWords * N]*/, 
                uint32_t N, 
                uint32_t r,
                uint32_t numSegments, 
                scrypt_mix_word_t *Links, 
                uint32_t *linkLengths, 
                uint32_t *numLinks, 
                uint32_t W, 
                uint128_t T, 
                uint32_t *currentLinkLength, 
                uint128_t Target, 
                uint8_t *pfWorking,
                uint8_t *pResetSignal) {}
		static twist_ROMixfn_work twist_getROMix_work(void) { 
            return twist_ROMix_work_error; }
		static void FASTCALL twist_ROMix_verify_error(
                scrypt_mix_word_t *X/*[chunkWords]*/, 
                scrypt_mix_word_t *Y, 
                scrypt_mix_word_t *V/*[chunkWords * ?]*/, 
                uint8_t *VSeeds, 
                uint32_t startSeed, 
                uint32_t numSeedsToUse, 
                uint32_t N, 
                uint32_t r,
                uint32_t numSegments, 
                scrypt_mix_word_t *Links, 
                uint32_t *linkLengths, 
                uint32_t *numLinks, 
                uint32_t startCheckLink, 
                uint32_t endCheckLink, 
                uint128_t T, 
                uint128_t Target, 
                uint32_t *workStep, 
                uint32_t *link,
                uint32_t *j,
                uint32_t numStepsToRecord,
                uint32_t *stepsToRecord,
                scrypt_mix_word_t *recordedSteps) {}
		static twist_ROMixfn_verify twist_getROMix_verify(void) { 
            return twist_ROMix_verify_error; }
		static void FASTCALL twist_ROMix_checkrecordedsteps_error(
                scrypt_mix_word_t *X/*[chunkWords]*/, 
                scrypt_mix_word_t *Y, 
                scrypt_mix_word_t *V/*[chunkWords * ?]*/, 
                uint8_t *VSeeds, 
                uint32_t startSeed, 
                uint32_t numSeedsToUse, 
                uint32_t N, 
                uint32_t r,
                uint32_t numSegments, 
                scrypt_mix_word_t *Links, 
                uint32_t *linkLengths, 
                uint32_t *numLinks, 
                uint128_t T, 
                uint128_t Target, 
                uint32_t *workStep, 
                uint32_t *link,
                uint32_t *j,
                uint32_t numStepsToRecord,
                uint32_t *stepsToRecord,
                scrypt_mix_word_t *newRecordedSteps
                uint32_t numRecordedOldSteps,
                uint32_t *oldRecordedStepPositions,
                scrypt_mix_word_t *oldRecordedSteps) {}
		static twist_ROMixfn_checkrecordedsteps 
        twist_getROMix_checkrecordedsteps(void) { 
            return twist_ROMix_checkrecordedsteps_error; }
		static void FASTCALL twist_ROMix_quickcheck_error(
                scrypt_mix_word_t *X/*[chunkWords]*/, 
                scrypt_mix_word_t *Y, 
                scrypt_mix_word_t *V/*[chunkWords * ?]*/, 
                uint8_t *VSeeds, 
                uint32_t N, 
                uint32_t r,
                uint32_t numSegments, 
                scrypt_mix_word_t *Links, 
                uint32_t *linkLengths, 
                uint32_t *numLinks, 
                uint128_t T, 
                uint128_t Target, 
                uint32_t *workStep, 
                uint32_t *link) {}
		static twist_ROMixfn_quickcheck twist_getROMix_quickcheck(void) { 
            return twist_ROMix_quickcheck_error; }
	#else
	#endif
	static int scrypt_test_mix(void) { return 0; }
	#error must define a mix function!
#endif

#if !defined(SCRYPT_CHOOSE_COMPILETIME)
#undef SCRYPT_MIX
#define SCRYPT_MIX SCRYPT_MIX_BASE
#endif
