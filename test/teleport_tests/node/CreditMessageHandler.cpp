#include <src/credits/creditsign.h>
#include <src/crypto/secp256k1point.h>
#include "CreditMessageHandler.h"
#include "test/teleport_tests/node/data_handler/DataMessageHandler.h"

#include "ListExpansionRequestMessage.h"

#include "log.h"
#include "TeleportNetworkNode.h"

#define LOG_CATEGORY "CreditMessageHandler.cpp"

using std::vector;
using std::string;
using std::set;

void CreditMessageHandler::HandleMinedCreditMessage(MinedCreditMessage msg)
{
    if (MinedCreditMessageWasRegardedAsValidAndHandled(msg))
        return;

    credit_system->StoreMinedCreditMessage(msg);

    if (msg.mined_credit.network_state.batch_number > 1 and not PreviousMinedCreditMessageWasHandled(msg))
    {
        QueueMinedCreditMessageBehindPrevious(msg);
        if (not EnclosedMessagesArePresent(msg))
        {
            FetchFailedListExpansion(msg);
            return;
        }
    }

    if (not EnclosedMessagesArePresent(msg))
    {
        FetchFailedListExpansion(msg);
        return;
    }

    if (not MinedCreditMessagePassesVerification(msg))
    {
        return;
    }

    Broadcast(msg);

    if (do_spot_checks and not ProofOfWorkPassesSpotCheck(msg))
        Broadcast(GetBadBatchMessage(msg.GetHash160()));
    else
    {
        LOCK(calendar_mutex);
        HandleValidMinedCreditMessage(msg);
        HandleQueuedMinedCreditMessages(msg);
    }
}

bool CreditMessageHandler::MinedCreditMessageWasRegardedAsValidAndHandled(MinedCreditMessage& msg)
{
    uint160 msg_hash = msg.GetHash160();
    return credit_system->MinedCreditWasRecordedToHaveTotalWork(msg_hash, msg.mined_credit.ReportedWork())
            and credit_system->MinedCreditMessageWasHandled(msg_hash);
}

bool CreditMessageHandler::PreviousMinedCreditMessageWasHandled(MinedCreditMessage& msg)
{
    uint160 previous_msg_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
    return credit_system->MinedCreditMessageWasHandled(previous_msg_hash);
}

void CreditMessageHandler::QueueMinedCreditMessageBehindPrevious(MinedCreditMessage& msg)
{
    uint160 msg_hash = msg.GetHash160();
    uint160 previous_msg_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
    std::vector<uint160> queued_messages = creditdata[previous_msg_hash]["queued"];
    if (not VectorContainsEntry(queued_messages, msg_hash))
        queued_messages.push_back(msg_hash);
    creditdata[previous_msg_hash]["queued"] = queued_messages;
}

void CreditMessageHandler::HandleQueuedMinedCreditMessages(MinedCreditMessage& msg)
{
    uint160 msg_hash = msg.GetHash160();
    std::vector<uint160> queued_messages = creditdata[msg_hash]["queued"];
    auto final_queued_messages = queued_messages;
    for (auto queued_msg_hash : queued_messages)
    {
        EraseEntryFromVector(queued_msg_hash, final_queued_messages);
        creditdata[msg_hash]["queued"] = final_queued_messages;
        HandleMessage(queued_msg_hash);
    }
}

bool CreditMessageHandler::EnclosedMessagesArePresent(MinedCreditMessage msg)
{
    return msg.hash_list.RecoverFullHashes(msgdata);
}

bool CreditMessageHandler::MinedCreditMessagePassesVerification(MinedCreditMessage& msg)
{
    uint160 msg_hash = msg.GetHash160();
    uint160 previous_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;

    if (msg.mined_credit.amount != ONE_CREDIT)
        return RejectMessage(msg_hash);

    if (msg.mined_credit.network_state.network_id != config.Uint64("network_id"))
        return RejectMessage(msg_hash);

    if (not mined_credit_message_validator.ValidateNetworkState(msg))
        return RejectMessage(msg_hash);

    if (not credit_system->QuickCheckProofOfWorkInMinedCreditMessage(msg))
    {
        return RejectMessage(msg_hash);
    }

    bool predecessor_passed_verification = creditdata[previous_hash]["passed_verification"];

    bool first_message = creditdata[msg_hash]["first_in_data_message"];

    if (msg.mined_credit.network_state.batch_number != 1 and not first_message)
    {
        bool have_data_for_preceding_mined_credit = msgdata[previous_hash].HasProperty("msg");
        MinedCreditMessage prev_msg = msgdata[previous_hash]["msg"];
        if (have_data_for_preceding_mined_credit or not credit_system->IsCalend(msg_hash))
            if (not predecessor_passed_verification)
            {
                return false;
            }
    }
    creditdata[msg_hash]["passed_verification"] = true;
    return true;
}

void CreditMessageHandler::HandleValidMinedCreditMessage(MinedCreditMessage& msg)
{
    credit_system->AcceptMinedCreditMessageAsValidByRecordingTotalWorkAndParent(msg);
    SwitchToNewTipIfAppropriate();
    credit_system->MarkMinedCreditMessageAsHandled(msg.GetHash160());
}

MinedCreditMessage CreditMessageHandler::Tip()
{
    return msgdata[calendar->LastMinedCreditMessageHash()]["msg"];
}

void CreditMessageHandler::SwitchToNewTipIfAppropriate()
{
    vector<uint160> batches_with_the_most_work = credit_system->MostWorkBatches();

    uint160 current_tip = calendar->LastMinedCreditMessageHash();
    if (VectorContainsEntry(batches_with_the_most_work, current_tip))
    {
        return;
    }

    if (batches_with_the_most_work.size() == 0)
    {
        return;
    }

    uint160 new_tip = batches_with_the_most_work[0];
    SwitchToTip(new_tip);
}

void CreditMessageHandler::SwitchToTip(uint160 msg_hash_of_new_tip)
{
    MinedCreditMessage msg_at_new_tip = msgdata[msg_hash_of_new_tip]["msg"];
    uint160 current_tip = calendar->LastMinedCreditMessageHash();

    if (msg_at_new_tip.mined_credit.network_state.previous_mined_credit_message_hash == current_tip)
    {
        AddToTip(msg_at_new_tip);
    }
    else
    {
        SwitchToTipViaFork(msg_hash_of_new_tip);
    }
}

void CreditMessageHandler::SwitchToTipViaFork(uint160 new_tip)
{
    uint160 old_tip = calendar->LastMinedCreditMessageHash();
    uint160 current_tip = calendar->LastMinedCreditMessageHash();
    *spent_chain = credit_system->GetSpentChainOnOtherProngOfFork(*spent_chain, current_tip, new_tip);
    credit_system->SwitchMainChainToOtherBranchOfFork(current_tip, new_tip);
    *calendar = Calendar(new_tip, credit_system);
    UpdateAcceptedMessagesAfterFork(old_tip, new_tip);
    if (wallet != NULL)
        wallet->SwitchAcrossFork(old_tip, new_tip, credit_system);
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

void CreditMessageHandler::FetchFailedListExpansion(MinedCreditMessage msg)
{
    CNode *peer = GetPeer(msg.GetHash160());
    if (peer == NULL)
        return;
    ListExpansionRequestMessage request(msg);
    uint160 request_hash = request.GetHash160();
    msgdata[request_hash]["is_expansion_request"] = true;
    msgdata[request_hash]["list_expansion_request"] = request;
    peer->PushMessage("credit", "list_expansion_request", request);
}

void CreditMessageHandler::HandleListExpansionRequestMessage(ListExpansionRequestMessage request)
{
    CNode *peer = GetPeer(request.GetHash160());
    if (peer == NULL)
        return;

    ListExpansionMessage list_expansion_message(request, credit_system);
    peer->PushMessage("credit", "list_expansion", list_expansion_message);
}

void CreditMessageHandler::HandleListExpansionMessage(ListExpansionMessage list_expansion_message)
{
    if (not ValidateListExpansionMessage(list_expansion_message))
    {
        log_ << "failed to validate list expansion message\n";
        return;
    }
    HandleMessagesInListExpansionMessage(list_expansion_message);
    HandleMinedCreditMessageWithGivenListExpansion(list_expansion_message);
}

void CreditMessageHandler::HandleMinedCreditMessageWithGivenListExpansion(ListExpansionMessage list_expansion_message)
{
    ListExpansionRequestMessage request = msgdata[list_expansion_message.request_hash]["list_expansion_request"];
    MinedCreditMessage msg = msgdata[request.mined_credit_message_hash]["msg"];
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << std::string("credit") << std::string("msg") << msg;
    CNode *peer = GetPeer(list_expansion_message.GetHash160());
    HandleMessage(ss, peer);
}

void CreditMessageHandler::HandleMessagesInListExpansionMessage(ListExpansionMessage list_expansion_message)
{
    CNode *peer = GetPeer(list_expansion_message.GetHash160());
    for (uint32_t i = 0; i < list_expansion_message.message_types.size(); i++)
    {
        HandleMessageWithSpecifiedTypeAndContent(list_expansion_message.message_types[i],
                                                 list_expansion_message.message_contents[i],
                                                 peer);
    }
}

void CreditMessageHandler::HandleMessageWithSpecifiedTypeAndContent(std::string type, vch_t content, CNode *peer)
{
    CDataStream ss(content, SER_NETWORK, CLIENT_VERSION);
    CDataStream ssReceived(SER_NETWORK, CLIENT_VERSION);
    if (type == "msg")
    {
        MinedCreditMessage msg;
        ss >> msg;
        ssReceived << std::string("credit") << std::string("msg") << msg;
    }
    else if (type == "tx")
    {
        SignedTransaction tx;
        ss >> tx;
        ssReceived << std::string("credit") << std::string("tx") << tx;
    }
    HandleMessage(ssReceived, peer);
}

bool CreditMessageHandler::ValidateListExpansionMessage(ListExpansionMessage list_expansion_message)
{
    if (not msgdata[list_expansion_message.request_hash]["is_expansion_request"])
        return false;
    if (list_expansion_message.message_types.size() != list_expansion_message.message_contents.size())
        return false;
    MemoryDataStore hashdata;
    LoadHashesIntoDataStoreFromMessageTypesAndContents(hashdata,
                                                       list_expansion_message.message_types,
                                                       list_expansion_message.message_contents, credit_system);
    ListExpansionRequestMessage request = msgdata[list_expansion_message.request_hash]["list_expansion_request"];
    MinedCreditMessage msg = msgdata[request.mined_credit_message_hash]["msg"];
    return msg.hash_list.RecoverFullHashes(hashdata);
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
                                                                  calendar->LastMinedCreditMessageHash(),
                                                                  msg.GetHash160());
    calendar->AddToTip(msg, credit_system);
    if (wallet != NULL)
        wallet->AddBatchToTip(msg, credit_system);
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
    accepted_messages.push_back(msg.GetHash160());
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
            SignedTransaction accepted_tx = msgdata[hash]["tx"];
            for (auto input : accepted_tx.rawtx.inputs)
                spent_positions.insert(input.position);
        }
    return spent_positions;
}

bool CreditMessageHandler::CheckInputsAreUnspentByAcceptedTransactions(SignedTransaction tx)
{
    std::set<uint64_t> spent_positions = PositionsSpentByAcceptedTransactions();

    for (auto input : tx.rawtx.inputs)
    {
        if (spent_positions.count(input.position))
            return false;
    }
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

    if (VectorContainsEntry(accepted_messages, tx_hash))
    {
        return;
    }

    if (not VerifyTransactionSignature(tx))
    {
        RejectMessage(tx_hash);
        return;
    }

    if (transaction_validator.OutputsOverflow(tx) or transaction_validator.ContainsRepeatedInputs(tx))
    {
        RejectMessage(tx_hash);
        return;
    }

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

bool CreditMessageHandler::ProofOfWorkPassesSpotCheck(MinedCreditMessage &msg)
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
    {
        RejectMessage(bad_batch_message.GetHash160());
        return;
    }

    creditdata[msg_hash]["spot_check_failure"] = bad_batch_message.check;

    if (credit_system->IsInMainChain(msg_hash))
        RemoveFromMainChainAndSwitchToNewTip(msg_hash);
}

void CreditMessageHandler::RemoveFromMainChainAndSwitchToNewTip(uint160 msg_hash)
{
    credit_system->RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(msg_hash);
    SwitchToNewTipIfAppropriate();
}

Point GetNewPublicKey(MemoryDataStore& keydata)
{
    CBigNum private_key;
    private_key.Randomize(Secp256k1Point::Modulus());
    Point public_key(SECP256K1, private_key);
    keydata[public_key]["privkey"] = private_key;
    return public_key;
}

MinedCreditMessage CreditMessageHandler::GenerateMinedCreditMessageWithoutProofOfWork()
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

void CreditMessageHandler::SetNetworkNode(TeleportNetworkNode *teleport_network_node_)
{
    teleport_network_node = teleport_network_node_;
}

void CreditMessageHandler::SetCalendar(Calendar& calendar_)
{
    calendar = &calendar_;
//    for (auto calend : calendar->calends)
//    {
//        credit_system->AddToMainChain(calend);
//        credit_system->AcceptMinedCreditMessageAsValidByRecordingTotalWorkAndParent(calend);
//    }
//    for (auto mined_credit_message : calendar->current_diurn.credits_in_diurn)
//        credit_system->AddToMainChain(mined_credit_message);
}