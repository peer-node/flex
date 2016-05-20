#ifndef FLEX_CREDITSYSTEM_H
#define FLEX_CREDITSYSTEM_H


#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include <src/crypto/uint256.h>
#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <src/credits/SignedTransaction.h>
#include <src/credits/CreditBatch.h>
#include "MinedCreditMessage.h"

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
};


#endif //FLEX_CREDITSYSTEM_H
