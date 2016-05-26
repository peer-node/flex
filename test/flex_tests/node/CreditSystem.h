#ifndef FLEX_CREDITSYSTEM_H
#define FLEX_CREDITSYSTEM_H


#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include <src/crypto/uint256.h>
#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <src/credits/SignedTransaction.h>
#include <src/credits/CreditBatch.h>
#include "MinedCreditMessage.h"
#include "Calend.h"

#define TARGET_BATCH_INTERVAL 60000000ULL // one minute
#define TARGET_DIURN_LENGTH (24 * 60 * 60 * 1000000ULL) // one day

#define MEMORY_FACTOR 19

uint160 AdjustDifficultyAfterBatchInterval(uint160 earlier_difficulty, uint64_t interval);
uint160 AdjustDiurnalDifficultyAfterDiurnDuration(uint160 earlier_diurnal_difficulty, uint64_t duration);

class CreditSystem
{
public:
    MemoryDataStore &msgdata, &creditdata;

    CreditSystem(MemoryDataStore &msgdata, MemoryDataStore &creditdata):
            msgdata(msgdata), creditdata(creditdata)
    { }

    uint160 MostWorkBatch();

    void StoreMinedCredit(MinedCredit mined_credit);

    void StoreTransaction(SignedTransaction transaction);

    void StoreHash(uint160 hash);

    void StoreMinedCreditMessage(MinedCreditMessage msg);

    CreditBatch ReconstructBatch(MinedCreditMessage &msg);

    uint160 FindFork(uint160 credit_hash1, uint160 credit_hash2);

    uint160 PreviousCreditHash(uint160 credit_hash);

    std::set<uint64_t> GetPositionsOfCreditsSpentBetween(uint160 from_credit_hash, uint160 to_credit_hash);

    std::set<uint64_t> GetPositionsOfCreditsSpentInBatch(uint160 credit_hash);

    bool GetSpentAndUnspentWhenSwitchingAcrossFork(uint160 from_credit_hash, uint160 to_credit_hash,
                                                   std::set<uint64_t> &spent, std::set<uint64_t> &unspent);

    BitChain GetSpentChainOnOtherProngOfFork(BitChain &spent_chain, uint160 from_credit_hash, uint160 to_credit_hash);

    BitChain GetSpentChain(uint160 credit_hash);

    void AddToMainChain(MinedCreditMessage &msg);

    uint160 TotalWork(MinedCreditMessage &msg);

    void RemoveFromMainChain(MinedCreditMessage &msg);

    bool IsInMainChain(uint160 credit_hash);

    bool IsCalend(uint160 credit_hash);

    uint160 PrecedingCalendCreditHash(uint160 credit_hash);

    EncodedNetworkState SucceedingNetworkState(MinedCredit mined_credit);

    uint160 GetNextDifficulty(MinedCredit credit);

    uint160 GetNextDiurnalDifficulty(MinedCredit credit);

    void SetBatchRootAndSizeAndMessageListHash(MinedCreditMessage& msg);

    void SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(uint64_t number_of_megabytes);

    uint64_t expected_memory_factor_for_mined_credit_proofs_of_work{MEMORY_FACTOR};

    bool QuickCheckProofOfWorkInMinedCreditMessage(MinedCreditMessage &msg);

    bool QuickCheckProofOfWorkInCalend(Calend calend);

    bool CheckHashesSeedsAndThresholdsInMinedCreditMessageProofOfWork(MinedCreditMessage &msg);
};


#endif //FLEX_CREDITSYSTEM_H
