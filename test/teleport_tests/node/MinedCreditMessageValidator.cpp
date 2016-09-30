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
    auto previous_msg = GetPreviousMinedCreditMessage(msg);
    return msg.mined_credit.network_state.batch_offset == previous_msg.mined_credit.network_state.batch_offset
                                                            + previous_msg.mined_credit.network_state.batch_size;
}

bool MinedCreditMessageValidator::CheckBatchNumber(MinedCreditMessage &msg)
{
    auto previous_msg = GetPreviousMinedCreditMessage(msg);
    return msg.mined_credit.network_state.batch_number == previous_msg.mined_credit.network_state.batch_number + 1;
}

bool MinedCreditMessageValidator::CheckMessageListHash(MinedCreditMessage &msg)
{
    return msg.mined_credit.network_state.message_list_hash == msg.hash_list.GetHash160();
}

bool MinedCreditMessageValidator::CheckSpentChainHash(MinedCreditMessage &msg)
{
    uint160 msg_hash = msg.GetHash160();
    if (not credit_system->msgdata[msg_hash].HasProperty("msg"))
        credit_system->StoreMinedCreditMessage(msg);
    BitChain spent_chain = credit_system->GetSpentChain(msg_hash);
    return spent_chain.GetHash160() == msg.mined_credit.network_state.spent_chain_hash;
}

bool MinedCreditMessageValidator::CheckTimeStampSucceedsPreviousTimestamp(MinedCreditMessage &msg)
{
    auto previous_msg = GetPreviousMinedCreditMessage(msg);
    return msg.mined_credit.network_state.timestamp > previous_msg.mined_credit.network_state.timestamp;
}

bool MinedCreditMessageValidator::CheckTimeStampIsNotInFuture(MinedCreditMessage &msg)
{
    return msg.mined_credit.network_state.timestamp < GetAdjustedTimeMicros() + 2000000; // 2 seconds leeway
}

bool MinedCreditMessageValidator::CheckPreviousTotalWork(MinedCreditMessage &msg)
{
    auto previous_msg = GetPreviousMinedCreditMessage(msg);
    auto network_state = msg.mined_credit.network_state;
    auto prev_state = previous_msg.mined_credit.network_state;
    return network_state.previous_total_work == prev_state.previous_total_work + prev_state.difficulty;
}

MinedCreditMessage MinedCreditMessageValidator::GetPreviousMinedCreditMessage(MinedCreditMessage &msg)
{
    uint160 previous_msg_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
    return credit_system->msgdata[previous_msg_hash]["msg"];
}

bool MinedCreditMessageValidator::CheckDiurnalDifficulty(MinedCreditMessage &msg)
{
    auto previous_msg = GetPreviousMinedCreditMessage(msg);
    uint160 diurnal_difficulty = msg.mined_credit.network_state.diurnal_difficulty;
    return diurnal_difficulty == credit_system->GetNextDiurnalDifficulty(previous_msg);
}

bool MinedCreditMessageValidator::CheckDifficulty(MinedCreditMessage &msg)
{
    auto previous_msg = GetPreviousMinedCreditMessage(msg);
    uint160 difficulty = msg.mined_credit.network_state.difficulty;
    return difficulty == credit_system->GetNextDifficulty(previous_msg);
}

bool MinedCreditMessageValidator::CheckPreviousDiurnRoot(MinedCreditMessage &msg)
{
    auto previous_msg = GetPreviousMinedCreditMessage(msg);
    uint160 previous_diurn_root = msg.mined_credit.network_state.previous_diurn_root;
    return previous_diurn_root == credit_system->GetNextPreviousDiurnRoot(previous_msg);
}

bool MinedCreditMessageValidator::CheckDiurnalBlockRoot(MinedCreditMessage &msg)
{
    auto previous_msg = GetPreviousMinedCreditMessage(msg);

    uint160 diurnal_block_root = msg.mined_credit.network_state.diurnal_block_root;
    uint160 block_root_succeeding_previous = credit_system->GetNextDiurnalBlockRoot(previous_msg);

    return diurnal_block_root == block_root_succeeding_previous;
}

bool MinedCreditMessageValidator::CheckMessageListContainsPreviousMinedCreditHash(MinedCreditMessage &msg)
{
    EncodedNetworkState& state = msg.mined_credit.network_state;
    return (state.batch_number == 1 and state.previous_mined_credit_message_hash == 0) or
           VectorContainsEntry(msg.hash_list.full_hashes, state.previous_mined_credit_message_hash);
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
    if (msg.mined_credit.network_state.batch_number < 3)
        return true;
    uint160 previous_msg_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;

    MinedCreditMessage previous_msg = credit_system->msgdata[previous_msg_hash]["msg"];
    uint160 preceding_hash = previous_msg.mined_credit.network_state.previous_mined_credit_message_hash;

    return credit_system->msgdata[preceding_hash].HasProperty("msg");
}

bool MinedCreditMessageValidator::DataRequiredToCalculateDiurnalBlockRootIsPresent(MinedCreditMessage &msg)
{
    return credit_system->DataIsPresentFromMinedCreditToPrecedingCalendOrStart(msg);
}

