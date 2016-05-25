#include "MinedCreditMessageValidator.h"

bool MinedCreditMessageValidator::HasMissingData(MinedCreditMessage& msg)
{
    bool succeeded = msg.hash_list.RecoverFullHashes(credit_system->msgdata);
    return not succeeded;
}

bool MinedCreditMessageValidator::CheckBatchRoot(MinedCreditMessage& msg)
{
    auto credit_batch = credit_system->ReconstructBatch(msg);
    return credit_batch.Root() == msg.mined_credit.network_state.batch_root;
}

bool MinedCreditMessageValidator::CheckBatchSize(MinedCreditMessage &msg)
{
    auto credit_batch = credit_system->ReconstructBatch(msg);
    return credit_batch.size() == msg.mined_credit.network_state.batch_size;
}

bool MinedCreditMessageValidator::CheckBatchOffset(MinedCreditMessage &msg)
{
    uint160 previous_credit_hash = msg.mined_credit.network_state.previous_mined_credit_hash;
    MinedCredit previous_credit = credit_system->creditdata[previous_credit_hash]["mined_credit"];
    return msg.mined_credit.network_state.batch_offset == previous_credit.network_state.batch_offset
                                                            + previous_credit.network_state.batch_size;
}

bool MinedCreditMessageValidator::CheckBatchNumber(MinedCreditMessage &msg)
{
    uint160 previous_credit_hash = msg.mined_credit.network_state.previous_mined_credit_hash;
    MinedCredit previous_credit = credit_system->creditdata[previous_credit_hash]["mined_credit"];
    return msg.mined_credit.network_state.batch_number == previous_credit.network_state.batch_number + 1;
}

bool MinedCreditMessageValidator::CheckMessageListHash(MinedCreditMessage &msg)
{
    return msg.mined_credit.network_state.message_list_hash == msg.hash_list.GetHash160();
}

bool MinedCreditMessageValidator::CheckSpentChainHash(MinedCreditMessage &msg)
{
    BitChain spent_chain = credit_system->GetSpentChain(msg.mined_credit.GetHash160());
    return spent_chain.GetHash160() == msg.mined_credit.network_state.spent_chain_hash;
}

