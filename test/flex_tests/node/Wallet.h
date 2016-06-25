#ifndef FLEX_WALLET_H
#define FLEX_WALLET_H


#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include <src/crypto/point.h>
#include <src/credits/SignedTransaction.h>
#include <src/credits/creditsign.h>
#include <src/crypto/key.h>
#include <src/base/BitcoinAddress.h>
#include "MinedCreditMessage.h"
#include "CreditSystem.h"

class Wallet
{
public:
    MemoryDataStore& keydata;
    std::vector<CreditInBatch> credits;

    Wallet(MemoryDataStore& keydata_):
        keydata(keydata_)
    { }

    std::vector<CreditInBatch> GetCredits();

    Point GetNewPublicKey();

    void HandleCreditInBatch(CreditInBatch credit_in_batch);

    bool PrivateKeyIsKnown(CreditInBatch credit_in_batch);

    bool HaveCreditInBatchAlready(CreditInBatch credit_in_batch);

    SignedTransaction GetSignedTransaction(Point pubkey, uint64_t amount);

    SignedTransaction GetSignedTransaction(uint160 keyhash, uint64_t amount);

    UnsignedTransaction GetUnsignedTransaction(vch_t key_data, uint64_t amount);

    void AddBatchToTip(MinedCreditMessage msg, CreditSystem *credit_system);

    void RemoveTransactionInputsSpentInBatchFromCredits(MinedCreditMessage &msg, CreditSystem *credit_system);

    void AddNewCreditsFromBatch(MinedCreditMessage &msg, CreditSystem *credit_system);

    uint64_t Balance();

    void RemoveBatchFromTip(MinedCreditMessage msg, CreditSystem *credit_system);

    void AddTransactionInputsSpentInRemovedBatchToCredits(MinedCreditMessage &msg, CreditSystem *credit_system);

    void RemoveCreditsFromRemovedBatch(MinedCreditMessage &msg, CreditSystem *credit_system);

    bool PrivateKeyIsKnown(uint160 key_hash);

    bool PrivateKeyIsKnown(Point public_key);

    void SwitchAcrossFork(uint160 old_tip, uint160 new_tip, CreditSystem *credit_system);
};

uint160 GetKeyHashFromAddress(std::string address_string);

std::string GetAddressFromPublicKey(Point public_key);

#endif //FLEX_WALLET_H
