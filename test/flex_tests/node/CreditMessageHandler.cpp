#include <src/credits/creditsign.h>
#include "CreditMessageHandler.h"
#include "CreditSystem.h"
#include "BadBatchMessage.h"

using std::vector;

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
        uint160 bad_batch_message_hash = GetBadBatchMessage(msg_hash).GetHash160();
        Broadcast(GetBadBatchMessage(msg_hash));
    }
    else
    {
        HandleValidMinedCreditMessage(msg);
    }
}

void CreditMessageHandler::HandleValidMinedCreditMessage(MinedCreditMessage& msg)
{
    credit_system->AcceptMinedCreditAsValidByRecordingTotalWorkAndParent(msg.mined_credit);
    SwitchToNewTipIfAppropriate();
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
    uint160 current_tip = calendar->LastMinedCreditHash();

    *spent_chain = credit_system->GetSpentChainOnOtherProngOfFork(*spent_chain, current_tip, new_tip);

    credit_system->SwitchMainChainToOtherBranchOfFork(current_tip, new_tip);
    *calendar = Calendar(new_tip, credit_system);
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
}

bool CreditMessageHandler::CheckInputsAreUnspentAccordingToSpentChain(SignedTransaction tx)
{
    for (auto input : tx.rawtx.inputs)
        if (input.position > spent_chain->length or spent_chain->Get(input.position))
            return false;
    return true;
}

bool CreditMessageHandler::CheckInputsAreUnspentByAcceptedTransactions(SignedTransaction tx)
{
    for (auto input : tx.rawtx.inputs)
        if (positions_spent_by_accepted_transactions.count(input.position))
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

    if (transaction_validator.OutputsOverflow(tx))
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
    for (auto input : tx.rawtx.inputs)
        positions_spent_by_accepted_transactions.insert(input.position);

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