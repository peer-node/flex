#include <src/base/util_time.h>
#include <test/teleport_tests/node/relays/structures/RelayState.h>
#include <test/teleport_tests/node/relays/handlers/RelayMessageHandler.h>
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
    if (msg.mined_credit.network_state.batch_number == 0)
        return false;
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
    log_ << "retrieved spent chain is:" << spent_chain.ToString() << "\n";
    log_ << "retrieved spent chain has hash:" << spent_chain.GetHash160() << "\n";
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
    log_ << "CheckDifficulty: specified difficulty is " << difficulty << "\n";
    log_ << "previous difficulty was " << previous_msg.mined_credit.network_state.difficulty << "\n";
    log_ << "difficulty after that should be: " << credit_system->GetNextDifficulty(previous_msg) << "\n";
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

    log_ << "ValidateNetworkState: msg is " << msg.json() << "\n";
    log_ << "initial difficulty is " << credit_system->initial_difficulty << "\n";
    ok &= CheckBatchNumber(msg);

    ok &= CheckPreviousTotalWork(msg);

    if (DataRequiredToCalculateDifficultyIsPresent(msg))
    {
        ok &= CheckDifficulty(msg);
        if (not CheckDifficulty(msg))
        {
            log_ << "bad difficulty\n";
        }
    }

    if (DataRequiredToCalculateDiurnalDifficultyIsPresent(msg))
    {
        ok &= CheckDiurnalDifficulty(msg);
        if (not CheckDiurnalDifficulty(msg))
        {
            log_ << "bad diurnal difficulty\n";
        }
    }

    ok &= CheckBatchOffset(msg);

    log_ << "checked batch offset, ok so far: " << ok << "\n";

    ok &= CheckTimeStampIsNotInFuture(msg);

    log_ << "checked time stamp, ok so far: " << ok << "\n";

    ok &= CheckPreviousDiurnRoot(msg);

    log_ << "checked previous diurn root, ok so far: " << ok << "\n";

    ok &= CheckPreviousCalendHash(msg);

    log_ << "checked previous calend hash, ok so far: " << ok << "\n";

    ok &= CheckTimeStampSucceedsPreviousTimestamp(msg);

    log_ << "checked TimeStampSucceedsPreviousTimestamp, ok so far: " << ok << "\n";

    ok &= CheckBatchRoot(msg);

    log_ << "checked batch root, ok so far: " << ok << "\n";

    ok &= CheckBatchSize(msg);

    log_ << "checked batch size, ok so far: " << ok << "\n";

    if (DataRequiredToCalculateDiurnalBlockRootIsPresent(msg))
    {
        ok &= CheckDiurnalBlockRoot(msg);
        log_ << "checked diurnal block root, ok so far: " << ok << "\n";
    }

    ok &= CheckSpentChainHash(msg);

    log_ << "checked spent chain hash, ok so far: " << ok << "\n";
    log_ << "spent chain hash is " << msg.mined_credit.network_state.spent_chain_hash << "\n";

    ok &= CheckMessageListHash(msg);

    log_ << "checked message list hash, ok so far: " << ok << "\n";

    ok &= CheckMessageListContainsPreviousMinedCreditHash(msg);

    log_ << "checked message list contains previous mined credit message hash, ok so far: " << ok << "\n";

    ok &= CheckRelayStateHash(msg);

    log_ << "checked relay state hash. ok: " << ok << "\n";

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

bool MinedCreditMessageValidator::DataRequiredToCalculateDiurnalDifficultyIsPresent(MinedCreditMessage &msg)
{
    uint160 previous_msg_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
    uint160 previous_calend_hash = msg.mined_credit.network_state.previous_calend_hash;

    if (previous_msg_hash != previous_calend_hash)
        return credit_system->msgdata[previous_msg_hash].HasProperty("msg");

    if (previous_calend_hash == 0)
        return true;

    MinedCreditMessage previous_msg = credit_system->msgdata[previous_msg_hash]["msg"];
    uint160 preceding_calend_hash = previous_msg.mined_credit.network_state.previous_calend_hash;
    MinedCreditMessage preceding_calend = credit_system->msgdata[preceding_calend_hash]["msg"];
    return credit_system->msgdata[preceding_calend_hash].HasProperty("msg");
}

bool MinedCreditMessageValidator::DataRequiredToCalculateDiurnalBlockRootIsPresent(MinedCreditMessage &msg)
{
    return credit_system->DataIsPresentFromMinedCreditToPrecedingCalendOrStart(msg);
}

bool MinedCreditMessageValidator::CheckPreviousCalendHash(MinedCreditMessage msg)
{
    uint160 previous_msg_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
    MinedCreditMessage previous_msg = credit_system->msgdata[previous_msg_hash]["msg"];

    if (credit_system->IsCalend(previous_msg_hash))
        return msg.mined_credit.network_state.previous_calend_hash == previous_msg_hash;

    return msg.mined_credit.network_state.previous_calend_hash ==
            previous_msg.mined_credit.network_state.previous_calend_hash;
}

bool MinedCreditMessageValidator::CheckRelayStateHash(MinedCreditMessage &msg)
{
    uint160 previous_msg_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
    log_ << "CheckRelayStateHash: from " <<  previous_msg_hash << " to " << msg.GetHash160() << "\n";
    MinedCreditMessage previous_msg = credit_system->msgdata[previous_msg_hash]["msg"];

    RelayMessageHandler handler(*data);
    handler.mode = BLOCK_VALIDATION;
    log_ << "previous relay state hash: " << previous_msg.mined_credit.network_state.relay_state_hash << "\n";
    handler.relay_state = data->GetRelayState(previous_msg.mined_credit.network_state.relay_state_hash);
    log_ << "previous relay state size: " << handler.relay_state.relays.size() << "\n";

    handler.SetCreditSystem(credit_system);
    handler.relay_state.latest_mined_credit_message_hash = previous_msg_hash;

    if (not msg.hash_list.RecoverFullHashes(data->msgdata))
        return false;

    for (auto &message_hash : msg.hash_list.full_hashes)
    {
        if (not handler.ValidateMessage(message_hash))
            return false;
        log_ << "passing message with hash: " << message_hash << " to handler\n";
        handler.HandleMessage(message_hash);
    }
    log_ << "handled all messages\n";
    log_ << "relay state size is now: " << handler.relay_state.relays.size() << "\n";
    if (handler.relay_state.GetHash160() != msg.mined_credit.network_state.relay_state_hash)
    {
        log_ << "relay state hashes don't match. validation failed\n";
        log_ << handler.relay_state.GetHash160() << " vs " << msg.mined_credit.network_state.relay_state_hash << "\n";
    }
    return handler.relay_state.GetHash160() == msg.mined_credit.network_state.relay_state_hash;
}

