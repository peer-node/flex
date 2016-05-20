#ifndef FLEX_NETWORKSTATE_H
#define FLEX_NETWORKSTATE_H


#include <test/flex_tests/mined_credits/EncodedNetworkState.h>
#include <src/credits/SignedTransaction.h>
#include <src/credits/CreditBatch.h>
#include <test/flex_tests/mined_credits/MinedCredit.h>

class NetworkState
{
public:
    CreditBatch credit_batch;
    MinedCredit previous_mined_credit;
    std::vector<SignedTransaction> transactions;

    EncodedNetworkState GetEncodedState();

    uint160 GetBatchRoot();

    void AddTransactionOutputsToCreditBatch(SignedTransaction transaction);

    void SetPreviousMinedCredit(MinedCredit credit);

    void AddTransaction(SignedTransaction transaction);

    bool ValidateIncomingMinedCredit(MinedCredit incoming_mined_credit);
};


#endif //FLEX_NETWORKSTATE_H
