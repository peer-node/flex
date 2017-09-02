#include <src/credits/creditsign.h>
#include <src/crypto/secp256k1point.h>
#include "CreditMessageHandler.h"
#include "test/teleport_tests/node/data/handlers/DataMessageHandler.h"
#include "test/teleport_tests/node/data/LoadHashesIntoDataStore.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"


#include "log.h"
#define LOG_CATEGORY "CreditMessageHandler.cpp"

using std::vector;
using std::string;
using std::set;

void CreditMessageHandler::SetTipController(TipController *tip_controller_)
{
    tip_controller = tip_controller_;
    using_internal_tip_controller = false;
}

void CreditMessageHandler::SetMinedCreditMessageBuilder(MinedCreditMessageBuilder *builder_)
{
    builder = builder_;
    using_internal_builder = false;
    if (tip_controller != NULL)
        tip_controller->SetMinedCreditMessageBuilder(builder);
}

void CreditMessageHandler::SetCreditSystem(CreditSystem *credit_system_)
{
    credit_system = credit_system_;
    mined_credit_message_validator.SetCreditSystem(credit_system);
    transaction_validator.SetCreditSystem(credit_system);
    tip_controller->SetCreditSystem(credit_system);
    builder->SetCreditSystem(credit_system);
}

void CreditMessageHandler::SetCalendar(Calendar& calendar_)
{
    calendar = &calendar_;
    tip_controller->SetCalendar(calendar);
    builder->SetCalendar(calendar);
}

void CreditMessageHandler::SetConfig(TeleportConfig& config_)
{
    config = config_;
    builder->SetConfig(config);
}

void CreditMessageHandler::SetSpentChain(BitChain& spent_chain_)
{
    spent_chain = &spent_chain_;
    tip_controller->SetSpentChain(spent_chain);
    builder->SetSpentChain(spent_chain);
}

void CreditMessageHandler::SetWallet(Wallet &wallet_)
{
    wallet = &wallet_;
    tip_controller->SetWallet(wallet);
    builder->SetWallet(wallet);
}

void CreditMessageHandler::SetNetworkNode(TeleportNetworkNode *teleport_network_node_)
{
    teleport_network_node = teleport_network_node_;
}

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
    vector<uint160> queued_messages = creditdata[previous_msg_hash]["queued"];
    if (not VectorContainsEntry(queued_messages, msg_hash))
        queued_messages.push_back(msg_hash);
    creditdata[previous_msg_hash]["queued"] = queued_messages;
}

void CreditMessageHandler::HandleQueuedMinedCreditMessages(MinedCreditMessage& msg)
{
    uint160 msg_hash = msg.GetHash160();
    vector<uint160> queued_messages = creditdata[msg_hash]["queued"];
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

    log_ << "checking network state\n";
    if (not mined_credit_message_validator.ValidateNetworkState(msg))
    {
        log_ << "bad network state\n";
        return RejectMessage(msg_hash);
    }
    log_ << "network state ok\n";

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
    tip_controller->SwitchToNewTipIfAppropriate();
    credit_system->MarkMinedCreditMessageAsHandled(msg.GetHash160());
    SendRelayJoinMessageIfAppropriate(msg);
}

void CreditMessageHandler::SendRelayJoinMessageIfAppropriate(MinedCreditMessage& msg)
{
    Point public_key = msg.mined_credit.PublicKey();
    if (keydata[public_key].HasProperty("privkey"))
    {
        if (teleport_network_node->relay_message_handler != NULL)
        {
            teleport_network_node->relay_message_handler->HandleNewTip(msg);
            teleport_network_node->relay_message_handler->SendRelayJoinMessage(msg);
        }
    }
}

MinedCreditMessage CreditMessageHandler::Tip()
{
    return calendar->LastMinedCreditMessage();
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

bool CreditMessageHandler::CheckInputsAreUnspentAccordingToSpentChain(SignedTransaction tx)
{
    for (auto input : tx.rawtx.inputs)
        if (input.position > spent_chain->length or spent_chain->Get(input.position))
            return false;
    return true;
}

bool CreditMessageHandler::CheckInputsAreUnspentByAcceptedTransactions(SignedTransaction tx)
{
    std::set<uint64_t> spent_positions = builder->PositionsSpentByAcceptedTransactions();

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

    if (VectorContainsEntry(builder->accepted_messages, tx_hash))
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
    builder->accepted_messages.push_back(tx.GetHash160());

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
        tip_controller->RemoveFromMainChainAndSwitchToNewTip(msg_hash);
}
