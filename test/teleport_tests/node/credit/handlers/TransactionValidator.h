#ifndef TELEPORT_TRANSACTIONVALIDATOR_H
#define TELEPORT_TRANSACTIONVALIDATOR_H


#include "test/teleport_tests/node/credit/structures/CreditSystem.h"

class TransactionValidator
{
public:
    CreditSystem* credit_system;

    void SetCreditSystem(CreditSystem *credit_system_);

    bool OutputsOverflow(SignedTransaction &tx);

    bool ContainsRepeatedInputs(SignedTransaction &tx);

    bool CheckIfCreditInBatchIsInTheMainChain(CreditInBatch &credit_in_batch);

    uint160 GetMinedCreditMessageHashFromCreditInBatch(CreditInBatch &credit_in_batch);

    bool CheckBranchOfCreditInBatch(CreditInBatch credit_in_batch);
};


#endif //TELEPORT_TRANSACTIONVALIDATOR_H
