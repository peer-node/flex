#ifndef FLEX_TRANSACTIONVALIDATOR_H
#define FLEX_TRANSACTIONVALIDATOR_H


#include "CreditSystem.h"

class TransactionValidator
{
public:
    CreditSystem* credit_system;

    void SetCreditSystem(CreditSystem *credit_system_);

    bool CheckForOverflow(SignedTransaction &tx);

    bool CheckIfCreditInBatchIsInTheMainChain(CreditInBatch &credit_in_batch);

    uint160 GetMinedCreditHashFromCreditInBatch(CreditInBatch &credit_in_batch);

    bool CheckBranchOfCreditInBatch(CreditInBatch credit_in_batch);
};


#endif //FLEX_TRANSACTIONVALIDATOR_H
