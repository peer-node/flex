// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef FLEX_WALLET
#define FLEX_WALLET 1

#include "base/util.h"
#include "credits/creditsign.h"
#include "database/data.h"
#include <algorithm>

uint256 TwistPasswordHash(vch_t input);

vch_t GetSalt();
void SetSalt(vch_t salt);

string_t FlexAddressFromPubKey(Point pubkey);

class Wallet
{
public:
    uint256 seed, derived_seed;
    std::vector<CreditInBatch> credits;
    std::vector<CreditInBatch> spent;

    Wallet() {}

    Wallet(uint256 seed);
    
    string_t ToString() const;
    
    Point NewKey();

    uint64_t Tradeable();

    CreditInBatch GetCreditWithAmount(uint64_t amount);

    void HandleTransaction(SignedTransaction tx);

    void UnhandleTransaction(SignedTransaction tx);

    std::vector<uint160> GetKeyHashes();
    
    void HandleReceivedCredits(vector<CreditInBatch> incoming_credits);
    
    bool HaveKeyForCredit(Credit credit);
    
    void HandleNewBatch(CreditBatch batch);

    void UnhandleBatch(CreditBatch batch);

    uint64_t Balance();

    bool SameKey(vch_t key1, vch_t key2);

    uint64_t Balance(vch_t keydata);

    uint64_t Balance(Point pubkey);

    bool AddDiurnBranch(CreditInBatch& credit);

    bool MakeTradeable(uint64_t amount);

    UnsignedTransaction GetUnsignedTxGivenKeyData(vch_t keydata,
                                                  uint64_t amount, 
                                                  bool trade);

    UnsignedTransaction GetUnsignedTxGivenPubKey(Point pubkey,
                                                 uint64_t amount,
                                                 bool trade);

    bool ImportPrivateKey(CBigNum privkey);

    void ImportWatchedCredits(uint160 keyhash);
};

#endif