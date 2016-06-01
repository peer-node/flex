#include <src/credits/creditsign.h>
#include "CreditMessageHandler.h"
#include "CreditSystem.h"


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