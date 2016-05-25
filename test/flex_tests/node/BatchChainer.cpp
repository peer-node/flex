#include "BatchChainer.h"
#include "MinedCreditMessageValidator.h"


void BatchChainer::SetCreditSystem(CreditSystem *credit_system_)
{
    credit_system = credit_system_;
}

uint160 BatchChainer::LatestMinedCreditHash()
{
    return latest_mined_credit_hash;
}

void BatchChainer::AddBatchToTip(MinedCreditMessage &msg)
{
    credit_system->StoreMinedCreditMessage(msg);
    latest_mined_credit_hash = msg.mined_credit.GetHash160();
}

void BatchChainer::RemoveBatchFromTip()
{
    latest_mined_credit_hash = credit_system->PreviousCreditHash(latest_mined_credit_hash);
}

void BatchChainer::SetMinedCreditMessageValidator(MinedCreditMessageValidator *mined_credit_message_validator_)
{
    mined_credit_message_validator = mined_credit_message_validator_;
}

bool BatchChainer::ValidateMinedCreditMessage(MinedCreditMessage &msg)
{
    bool batch_offset_ok = mined_credit_message_validator->CheckBatchOffset(msg);
    bool batch_number_ok = mined_credit_message_validator->CheckBatchNumber(msg);
    bool batch_root_ok = mined_credit_message_validator->CheckBatchRoot(msg);
    bool batch_size_ok = mined_credit_message_validator->CheckBatchSize(msg);
    bool message_list_hash_ok = mined_credit_message_validator->CheckMessageListHash(msg);
    bool spent_chain_hash_ok = mined_credit_message_validator->CheckSpentChainHash(msg);

    return false;
}









