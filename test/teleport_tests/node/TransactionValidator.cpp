#include "TransactionValidator.h"

void TransactionValidator::SetCreditSystem(CreditSystem *credit_system_)
{
    credit_system = credit_system_;
}

bool TransactionValidator::OutputsOverflow(SignedTransaction &tx)
{
    uint64_t total = 0;
    for (auto credit : tx.rawtx.outputs)
    {
        if (total + credit.amount < total)
            return true;
        total += credit.amount;
    }
    return false;
}

uint160 TransactionValidator::GetMinedCreditMessageHashFromCreditInBatch(CreditInBatch &credit_in_batch)
{
    uint160 batch_root = credit_in_batch.branch.back();
    return credit_system->creditdata[batch_root]["msg_hash"];
}

bool TransactionValidator::CheckIfCreditInBatchIsInTheMainChain(CreditInBatch &credit_in_batch)
{
    uint160 msg_hash = GetMinedCreditMessageHashFromCreditInBatch(credit_in_batch);
    return credit_system->IsInMainChain(msg_hash);
}

bool TransactionValidator::CheckBranchOfCreditInBatch(CreditInBatch credit_in_batch)
{
    Credit raw_credit(credit_in_batch.keydata, credit_in_batch.amount);
    uint160 batch_root = credit_in_batch.branch.back();
    vch_t raw_credit_data = raw_credit.getvch();

    return VerifyBranchFromOrderedHashTree((uint32_t) credit_in_batch.position, raw_credit_data,
                                           credit_in_batch.branch, batch_root);

}

bool TransactionValidator::ContainsRepeatedInputs(SignedTransaction &tx)
{
    std::set<uint64_t> set_of_input_positions = tx.InputPositions();
    return set_of_input_positions.size() < tx.rawtx.inputs.size();
}







