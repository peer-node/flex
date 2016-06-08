#include <src/credits/creditsign.h>
#include "CreditMessageHandler.h"
#include "CreditSystem.h"
#include "BadBatchMessage.h"

using std::vector;
using std::string;
using std::set;

void CreditMessageHandler::HandleMinedCreditMessage(MinedCreditMessage msg)
{
    uint160 msg_hash = msg.GetHash160();

    if (msg.mined_credit.amount != ONE_CREDIT)
        return RejectMessage(msg_hash);

    if (msg.mined_credit.network_state.network_id != config.Uint64("network_id"))
        return RejectMessage(msg_hash);

    if (not msg.hash_list.RecoverFullHashes(msgdata))
        return FetchFailedListExpansion(msg_hash);

    if (not mined_credit_message_validator.ValidateNetworkState(msg))
        return RejectMessage(msg_hash);

    if (not credit_system->QuickCheckProofOfWorkInMinedCreditMessage(msg))
        return RejectMessage(msg_hash);

    Broadcast(msg);
    credit_system->StoreMinedCreditMessage(msg);

    if (not PassesSpotCheckOfProofOfWork(msg))
    {
        Broadcast(GetBadBatchMessage(msg_hash));
    }
    else
        HandleValidMinedCreditMessage(msg);
}

void CreditMessageHandler::HandleValidMinedCreditMessage(MinedCreditMessage& msg)
{
    credit_system->AcceptMinedCreditAsValidByRecordingTotalWorkAndParent(msg.mined_credit);
    SwitchToNewTipIfAppropriate();
}

MinedCreditMessage CreditMessageHandler::Tip()
{
    return creditdata[calendar->LastMinedCreditHash()]["msg"];
}

void CreditMessageHandler::SwitchToNewTipIfAppropriate()
{
    vector<uint160> batches_with_the_most_work = credit_system->MostWorkBatches();

    uint160 current_tip = calendar->LastMinedCreditHash();
    if (VectorContainsEntry(batches_with_the_most_work, current_tip))
        return;

    if (batches_with_the_most_work.size() == 0)
        return;

    uint160 new_tip = batches_with_the_most_work[0];
    SwitchToTip(new_tip);
}

void CreditMessageHandler::SwitchToTip(uint160 credit_hash_of_new_tip)
{
    MinedCreditMessage msg_at_new_tip = creditdata[credit_hash_of_new_tip]["msg"];
    uint160 current_tip = calendar->LastMinedCreditHash();

    if (msg_at_new_tip.mined_credit.network_state.previous_mined_credit_hash == current_tip)
        AddToTip(msg_at_new_tip);
    else
        SwitchToTipViaFork(credit_hash_of_new_tip);
}

void CreditMessageHandler::SwitchToTipViaFork(uint160 new_tip)
{
    uint160 old_tip = calendar->LastMinedCreditHash();
    uint160 current_tip = calendar->LastMinedCreditHash();
    *spent_chain = credit_system->GetSpentChainOnOtherProngOfFork(*spent_chain, current_tip, new_tip);
    credit_system->SwitchMainChainToOtherBranchOfFork(current_tip, new_tip);
    *calendar = Calendar(new_tip, credit_system);
    UpdateAcceptedMessagesAfterFork(old_tip, new_tip);
}

void CreditMessageHandler::UpdateAcceptedMessagesAfterFork(uint160 old_tip, uint160 new_tip)
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

void CreditMessageHandler::UpdateAcceptedMessagesAfterFork(vector<uint160> messages_on_old_branch,
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

bool CreditMessageHandler::TransactionHasNoSpentInputs(SignedTransaction tx, set<uint160>& spent_positions)
{
    for (auto position : tx.InputPositions())
        if (spent_chain->Get(position) or spent_positions.count(position))
            return false;
    return true;
}

void CreditMessageHandler::ValidateAcceptedMessagesAfterFork()
{
    vector<uint160> incoming_messages = accepted_messages;
    set<uint160> spent_positions;

    accepted_messages.resize(0);

    for (auto hash : incoming_messages)
    {
        bool should_keep = true;
        string message_type = credit_system->MessageType(hash);
        if (message_type == "mined_credit")
            should_keep = false;
        if (message_type == "tx")
        {
            SignedTransaction tx = creditdata[hash]["tx"];
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

void CreditMessageHandler::FetchFailedListExpansion(uint160 msg_hash)
{
    CNode *peer = GetPeer(msg_hash);
    if (peer != NULL)
        peer->FetchFailedListExpansion(msg_hash);
}

void CreditMessageHandler::RejectMessage(uint160 msg_hash)
{
    msgdata[msg_hash]["rejected"] = true;
    CNode *peer = GetPeer(msg_hash);
    if (peer != NULL)
        peer->Misbehaving(30);
}

void CreditMessageHandler::SetCreditSystem(CreditSystem *credit_system_)
{
    credit_system = credit_system_;
    mined_credit_message_validator.SetCreditSystem(credit_system_);
    transaction_validator.SetCreditSystem(credit_system_);
}

void CreditMessageHandler::AddToTip(MinedCreditMessage &msg)
{
    credit_system->StoreMinedCreditMessage(msg);
    credit_system->AddToMainChain(msg);
    *spent_chain = credit_system->GetSpentChainOnOtherProngOfFork(*spent_chain,
                                                                  calendar->LastMinedCreditHash(),
                                                                  msg.mined_credit.GetHash160());
    calendar->AddToTip(msg, credit_system);
    UpdateAcceptedMessagesAfterNewTip(msg);
}

void CreditMessageHandler::UpdateAcceptedMessagesAfterNewTip(MinedCreditMessage &msg)
{
    msg.hash_list.RecoverFullHashes(msgdata);
    for (auto enclosed_hash : msg.hash_list.full_hashes)
    {
        if (VectorContainsEntry(accepted_messages, enclosed_hash))
            EraseEntryFromVector(enclosed_hash, accepted_messages);
    }
    accepted_messages.push_back(msg.mined_credit.GetHash160());
}

bool CreditMessageHandler::CheckInputsAreUnspentAccordingToSpentChain(SignedTransaction tx)
{
    for (auto input : tx.rawtx.inputs)
        if (input.position > spent_chain->length or spent_chain->Get(input.position))
            return false;
    return true;
}

std::set<uint64_t> CreditMessageHandler::PositionsSpentByAcceptedTransactions()
{
    std::set<uint64_t> spent_positions;
    for (auto hash : accepted_messages)
        if (credit_system->MessageType(hash) == "tx")
        {
            SignedTransaction accepted_tx = creditdata[hash]["tx"];
            for (auto input : accepted_tx.rawtx.inputs)
                spent_positions.insert(input.position);
        }
    return spent_positions;
}

bool CreditMessageHandler::CheckInputsAreUnspentByAcceptedTransactions(SignedTransaction tx)
{
    std::set<uint64_t> spent_positions = PositionsSpentByAcceptedTransactions();
    for (auto input : tx.rawtx.inputs)
        if (spent_positions.count(input.position))
            return false;
    return true;
}

bool CreditMessageHandler::CheckInputsAreOKAccordingToCalendar(SignedTransaction tx)
{
    for (auto input : tx.rawtx.inputs)
        if (not calendar->ValidateCreditInBatch(input))
            return false;
    return true;
}

void CreditMessageHandler::HandleSignedTransaction(SignedTransaction tx)
{
    uint160 tx_hash = tx.GetHash160();

    if (VectorContainsEntry(accepted_messages, tx_hash));

    if (not VerifyTransactionSignature(tx))
        return RejectMessage(tx_hash);

    if (transaction_validator.OutputsOverflow(tx) or transaction_validator.ContainsRepeatedInputs(tx))
        return RejectMessage(tx_hash);

    if (not CheckInputsAreUnspentAccordingToSpentChain(tx))
        return;

    if (not CheckInputsAreUnspentByAcceptedTransactions(tx))
        return;

    if (not CheckInputsAreOKAccordingToCalendar(tx))
        return;

    AcceptTransaction(tx);
}

void CreditMessageHandler::AcceptTransaction(SignedTransaction tx)
{
    credit_system->StoreTransaction(tx);
    accepted_messages.push_back(tx.GetHash160());

    Broadcast(tx);
}

bool CreditMessageHandler::PassesSpotCheckOfProofOfWork(MinedCreditMessage &msg)
{
    if (not credit_system->ProofHasCorrectNumberOfSeedsAndLinks(msg.proof_of_work.proof))
        return false;
    uint160 msg_hash = msg.GetHash160();
    if (creditdata[msg_hash].HasProperty("spot_check_failure"))
        return false;

    TwistWorkCheck check = msg.proof_of_work.proof.SpotCheck();
    if (check.Valid())
        return true;

    creditdata[msg_hash]["spot_check_failure"] = check;
    return false;
}

BadBatchMessage CreditMessageHandler::GetBadBatchMessage(uint160 msg_hash)
{
    BadBatchMessage bad_batch_message;
    bad_batch_message.mined_credit_message_hash = msg_hash;
    bad_batch_message.check = creditdata[msg_hash]["spot_check_failure"];
    return bad_batch_message;
}

void CreditMessageHandler::HandleBadBatchMessage(BadBatchMessage bad_batch_message)
{
    uint160 msg_hash = bad_batch_message.mined_credit_message_hash;
    MinedCreditMessage msg = msgdata[msg_hash]["msg"];
    if (not bad_batch_message.check.VerifyInvalid(msg.proof_of_work.proof))
        return RejectMessage(bad_batch_message.GetHash160());

    creditdata[msg_hash]["spot_check_failure"] = bad_batch_message.check;
    uint160 credit_hash = msg.mined_credit.GetHash160();
    if (credit_system->IsInMainChain(credit_hash))
        RemoveFromMainChainAndSwitchToNewTip(credit_hash);
}

void CreditMessageHandler::RemoveFromMainChainAndSwitchToNewTip(uint160 credit_hash)
{
    credit_system->RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(credit_hash);
    SwitchToNewTipIfAppropriate();
}