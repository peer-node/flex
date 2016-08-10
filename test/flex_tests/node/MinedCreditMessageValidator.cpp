#include <src/base/util_time.h>
#include "MinedCreditMessageValidator.h"

#include "log.h"
#define LOG_CATEGORY "MinedCreditMessageValidator.cpp"

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
    auto previous_mined_credit = GetPreviousMinedCredit(msg);
    return msg.mined_credit.network_state.batch_offset == previous_mined_credit.network_state.batch_offset
                                                            + previous_mined_credit.network_state.batch_size;
}

bool MinedCreditMessageValidator::CheckBatchNumber(MinedCreditMessage &msg)
{
    auto previous_mined_credit = GetPreviousMinedCredit(msg);
    return msg.mined_credit.network_state.batch_number == previous_mined_credit.network_state.batch_number + 1;
}

bool MinedCreditMessageValidator::CheckMessageListHash(MinedCreditMessage &msg)
{
    return msg.mined_credit.network_state.message_list_hash == msg.hash_list.GetHash160();
}

bool MinedCreditMessageValidator::CheckSpentChainHash(MinedCreditMessage &msg)
{
    uint160 credit_hash = msg.mined_credit.GetHash160();
    if (not credit_system->creditdata[credit_hash].HasProperty("msg"))
        credit_system->StoreMinedCreditMessage(msg);
    BitChain spent_chain = credit_system->GetSpentChain(credit_hash);
    return spent_chain.GetHash160() == msg.mined_credit.network_state.spent_chain_hash;
}

bool MinedCreditMessageValidator::CheckTimeStampSucceedsPreviousTimestamp(MinedCreditMessage &msg)
{
    auto previous_mined_credit = GetPreviousMinedCredit(msg);
    return msg.mined_credit.network_state.timestamp > previous_mined_credit.network_state.timestamp;
}

bool MinedCreditMessageValidator::CheckTimeStampIsNotInFuture(MinedCreditMessage &msg)
{
    return msg.mined_credit.network_state.timestamp < GetTimeMicros();
}

bool MinedCreditMessageValidator::CheckPreviousTotalWork(MinedCreditMessage &msg)
{
    auto previous_mined_credit = GetPreviousMinedCredit(msg);
    auto network_state = msg.mined_credit.network_state;
    auto prev_state = previous_mined_credit.network_state;
    return network_state.previous_total_work == prev_state.previous_total_work + prev_state.difficulty;
}

MinedCredit MinedCreditMessageValidator::GetPreviousMinedCredit(MinedCreditMessage &msg)
{
    uint160 previous_credit_hash = msg.mined_credit.network_state.previous_mined_credit_hash;
    return credit_system->creditdata[previous_credit_hash]["mined_credit"];
}

bool MinedCreditMessageValidator::CheckDiurnalDifficulty(MinedCreditMessage &msg)
{
    auto previous_mined_credit = GetPreviousMinedCredit(msg);
    uint160 diurnal_difficulty = msg.mined_credit.network_state.diurnal_difficulty;
    return diurnal_difficulty == credit_system->GetNextDiurnalDifficulty(previous_mined_credit);
}

bool MinedCreditMessageValidator::CheckDifficulty(MinedCreditMessage &msg)
{
    auto previous_mined_credit = GetPreviousMinedCredit(msg);
    uint160 difficulty = msg.mined_credit.network_state.difficulty;
    return difficulty == credit_system->GetNextDifficulty(previous_mined_credit);
}

bool MinedCreditMessageValidator::CheckPreviousDiurnRoot(MinedCreditMessage &msg)
{
    auto previous_mined_credit = GetPreviousMinedCredit(msg);
    uint160 previous_diurn_root = msg.mined_credit.network_state.previous_diurn_root;
    return previous_diurn_root == credit_system->GetNextPreviousDiurnRoot(previous_mined_credit);
}

bool MinedCreditMessageValidator::CheckDiurnalBlockRoot(MinedCreditMessage &msg)
{
    auto previous_mined_credit = GetPreviousMinedCredit(msg);

    uint160 diurnal_block_root = msg.mined_credit.network_state.diurnal_block_root;
    uint160 block_root_succeeding_previous = credit_system->GetNextDiurnalBlockRoot(previous_mined_credit);

    return diurnal_block_root == block_root_succeeding_previous;
}

bool MinedCreditMessageValidator::CheckMessageListContainsPreviousMinedCreditHash(MinedCreditMessage &msg)
{
    EncodedNetworkState& state = msg.mined_credit.network_state;
    return (state.batch_number == 1 and state.previous_mined_credit_hash == 0) or
           VectorContainsEntry(msg.hash_list.full_hashes, state.previous_mined_credit_hash);
}

bool MinedCreditMessageValidator::ValidateNetworkState(MinedCreditMessage &msg)
{
    bool ok = true;
    uint32_t& batch_number = msg.mined_credit.network_state.batch_number;

    ok &= batch_number > 0;

    ok &= CheckBatchNumber(msg);

    ok &= CheckPreviousTotalWork(msg);

    if (DataRequiredToCalculateDifficultyIsPresent(msg))
    {
        ok &= CheckDifficulty(msg);
    }

    ok &= CheckDiurnalDifficulty(msg);

    ok &= CheckBatchOffset(msg);

    ok &= CheckTimeStampIsNotInFuture(msg);

    ok &= CheckPreviousDiurnRoot(msg);

    ok &= CheckTimeStampSucceedsPreviousTimestamp(msg);

    ok &= CheckBatchRoot(msg);

    ok &= CheckBatchSize(msg);

    if (DataRequiredToCalculateDiurnalBlockRootIsPresent(msg))
    {
        ok &= CheckDiurnalBlockRoot(msg);
    }

    ok &= CheckSpentChainHash(msg);

    ok &= CheckMessageListHash(msg);

    ok &= CheckMessageListContainsPreviousMinedCreditHash(msg);

    return ok;
}

bool MinedCreditMessageValidator::DataRequiredToCalculateDifficultyIsPresent(MinedCreditMessage &msg)
{
    if (msg.mined_credit.network_state.batch_number < 4)
        return true;
    uint160 previous_credit_hash = msg.mined_credit.network_state.previous_mined_credit_hash;

    MinedCredit previous_credit = credit_system->creditdata[previous_credit_hash]["msg"];
    uint160 preceding_hash = previous_credit.network_state.previous_mined_credit_hash;

    return credit_system->creditdata[preceding_hash].HasProperty("mined_credit");
}

bool MinedCreditMessageValidator::DataRequiredToCalculateDiurnalBlockRootIsPresent(MinedCreditMessage &msg)
{
    return credit_system->DataIsPresentFromMinedCreditToPrecedingCalendOrStart(msg.mined_credit);
}

