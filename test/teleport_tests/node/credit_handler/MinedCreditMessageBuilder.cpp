#include "MinedCreditMessageBuilder.h"
#include <src/vector_tools.h>
#include <test/teleport_tests/node/calendar/Calendar.h>
#include <src/crypto/secp256k1point.h>


#include "log.h"
#define LOG_CATEGORY "MinedCreditMessageBuilder.cpp"

using std::vector;
using std::set;
using std::string;


void MinedCreditMessageBuilder::SetConfig(TeleportConfig& config_) { config = config_; }

void MinedCreditMessageBuilder::SetCreditSystem(CreditSystem *credit_system_) { credit_system = credit_system_; }

void MinedCreditMessageBuilder::SetCalendar(Calendar *calendar_) { calendar = calendar_; }

void MinedCreditMessageBuilder::SetSpentChain(BitChain *spent_chain_) { spent_chain = spent_chain_; }

void MinedCreditMessageBuilder::SetWallet(Wallet *wallet_) { wallet = wallet_; }

MinedCreditMessage MinedCreditMessageBuilder::Tip()
{
    return calendar->LastMinedCreditMessage();
}

std::set<uint64_t> MinedCreditMessageBuilder::PositionsSpentByAcceptedTransactions()
{
    std::set<uint64_t> spent_positions;
    for (auto hash : accepted_messages)
        if (credit_system->MessageType(hash) == "tx")
        {
            SignedTransaction accepted_tx = msgdata[hash]["tx"];
            for (auto input : accepted_tx.rawtx.inputs)
                spent_positions.insert(input.position);
        }
    return spent_positions;
}

void MinedCreditMessageBuilder::UpdateAcceptedMessagesAfterNewTip(MinedCreditMessage &msg)
{
    msg.hash_list.RecoverFullHashes(msgdata);
    for (auto enclosed_hash : msg.hash_list.full_hashes)
    {
        if (VectorContainsEntry(accepted_messages, enclosed_hash))
            EraseEntryFromVector(enclosed_hash, accepted_messages);
    }
    accepted_messages.push_back(msg.GetHash160());
}

void MinedCreditMessageBuilder::UpdateAcceptedMessagesAfterFork(uint160 old_tip, uint160 new_tip)
{
    vector<uint160> messages_on_old_branch, messages_on_new_branch;
    credit_system->GetMessagesOnOldAndNewBranchesOfFork(old_tip, new_tip, messages_on_old_branch, messages_on_new_branch);
    UpdateAcceptedMessagesAfterFork(messages_on_old_branch, messages_on_new_branch);
}

void GetMessagesLostAndAdded(vector<uint160>& messages_lost_from_old_branch,
                             vector<uint160>& new_messages_on_new_branch,
                             vector<uint160> messages_on_old_branch,
                             vector<uint160> messages_on_new_branch)
{
    for (auto hash : messages_on_old_branch)
        if (not VectorContainsEntry(messages_on_new_branch, hash))
            messages_lost_from_old_branch.push_back(hash);
    for (auto hash : messages_on_new_branch)
        if (not VectorContainsEntry(messages_on_old_branch, hash))
            new_messages_on_new_branch.push_back(hash);
}

void MinedCreditMessageBuilder::UpdateAcceptedMessagesAfterFork(vector<uint160> messages_on_old_branch,
                                                                vector<uint160> messages_on_new_branch)
{
    vector<uint160> messages_lost_from_old_branch, new_messages_on_new_branch;
    GetMessagesLostAndAdded(messages_lost_from_old_branch, new_messages_on_new_branch,
                            messages_on_old_branch, messages_on_new_branch);

    for (auto hash : new_messages_on_new_branch)
        if (VectorContainsEntry(accepted_messages, hash))
            EraseEntryFromVector(hash, accepted_messages);

    for (auto hash : messages_lost_from_old_branch)
        if (not VectorContainsEntry(accepted_messages, hash))
            accepted_messages.push_back(hash);

    ValidateAcceptedMessagesAfterFork();
}

bool MinedCreditMessageBuilder::TransactionHasNoSpentInputs(SignedTransaction tx, set<uint160>& spent_positions)
{
    for (auto position : tx.InputPositions())
        if (spent_chain->Get(position) or spent_positions.count(position))
            return false;
    return true;
}

void MinedCreditMessageBuilder::ValidateAcceptedMessagesAfterFork()
{
    vector<uint160> incoming_messages = accepted_messages;
    set<uint160> spent_positions;

    accepted_messages.resize(0);

    for (auto hash : incoming_messages)
    {
        bool should_keep = true;
        string message_type = credit_system->MessageType(hash);
        if (message_type == "msg")
            should_keep = false;
        if (message_type == "tx")
        {
            SignedTransaction tx = msgdata[hash]["tx"];
            if (TransactionHasNoSpentInputs(tx, spent_positions))
                for (auto input : tx.rawtx.inputs)
                    spent_positions.insert(input.position);
            else
                should_keep = false;
        }
        if (should_keep)
            accepted_messages.push_back(hash);
    }
}

Point GetNewPublicKey(MemoryDataStore& keydata)
{
    CBigNum private_key;
    private_key.Randomize(Secp256k1Point::Modulus());
    Point public_key(SECP256K1, private_key);
    keydata[public_key]["privkey"] = private_key;
    return public_key;
}

MinedCreditMessage MinedCreditMessageBuilder::GenerateMinedCreditMessageWithoutProofOfWork()
{
    MinedCreditMessage msg, current_tip = Tip();

    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(current_tip);
    msg.mined_credit.keydata = GetNewPublicKey(keydata).getvch();

    if (msg.mined_credit.network_state.batch_number > 1)
        msg.hash_list.full_hashes.push_back(msg.mined_credit.network_state.previous_mined_credit_message_hash);
    else
        msg.mined_credit.network_state.network_id = config.Uint64("network_id");

    for (auto hash : accepted_messages)
        if (credit_system->MessageType(hash) == "tx")
            msg.hash_list.full_hashes.push_back(hash);
    msg.hash_list.GenerateShortHashes();

    credit_system->SetBatchRootAndSizeAndMessageListHashAndSpentChainHash(msg);
    return msg;
}