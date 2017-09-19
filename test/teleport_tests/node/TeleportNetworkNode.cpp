#include "TeleportNetworkNode.h"

#include "log.h"
#define LOG_CATEGORY "TeleportNetworkNode.cpp"

Calendar TeleportNetworkNode::GetCalendar()
{
    return calendar;
}

uint64_t TeleportNetworkNode::Balance()
{
    return wallet->Balance();
}

void TeleportNetworkNode::HandleMessage(std::string channel, CDataStream stream, CNode *peer)
{
    RecordReceiptOfMessage(channel, stream);

    if (channel == "credit" and credit_message_handler != NULL)
        credit_message_handler->HandleMessage(stream, peer);

    if (channel == "data" and data_message_handler != NULL)
        data_message_handler->HandleMessage(stream, peer);

    if (channel == "relay" and relay_message_handler != NULL)
        relay_message_handler->HandleMessage(stream, peer);

    if (channel == "deposit" and deposit_message_handler != NULL)
        deposit_message_handler->HandleMessage(stream, peer);
}

void TeleportNetworkNode::RecordReceiptOfMessage(std::string channel, CDataStream stream)
{
    uint256 message_hash = Hash(stream.begin(), stream.end());
    if (communicator != NULL and communicator->network.main_routine != NULL)
        communicator->network.main_routine->invs_received.insert(CInv(MSG_GENERAL, message_hash));
}

MinedCreditMessage TeleportNetworkNode::Tip()
{
    return credit_message_handler->Tip();
}

MinedCreditMessage TeleportNetworkNode::GenerateMinedCreditMessageWithoutProofOfWork()
{
    MinedCreditMessage msg = builder.GenerateMinedCreditMessageWithoutProofOfWork();
    creditdata[msg.mined_credit.GetHash()]["generated_msg"] = msg;
    return msg;
}

MinedCreditMessage TeleportNetworkNode::RecallGeneratedMinedCreditMessage(uint256 credit_hash)
{
    return creditdata[credit_hash]["generated_msg"];
}

void TeleportNetworkNode::HandleNewProof(NetworkSpecificProofOfWork proof)
{
    uint256 credit_hash = proof.branch[0];
    auto msg = RecallGeneratedMinedCreditMessage(credit_hash);
    msg.proof_of_work = proof;
    credit_message_handler->HandleMinedCreditMessage(msg);
}

uint160 TeleportNetworkNode::SendCreditsToPublicKey(Point public_key, uint64_t amount)
{
    SignedTransaction tx = wallet->GetSignedTransaction(public_key, amount);
    credit_message_handler->HandleSignedTransaction(tx);
    return tx.GetHash160();
}

Point TeleportNetworkNode::GetNewPublicKey()
{
    return wallet->GetNewPublicKey();
}

uint160 TeleportNetworkNode::SendToAddress(std::string address, int64_t amount)
{
    uint160 key_hash = GetKeyHashFromAddress(address);
    SignedTransaction tx = wallet->GetSignedTransaction(key_hash, (uint64_t) amount);
    credit_message_handler->HandleSignedTransaction(tx);
    return tx.GetHash160();
}

void TeleportNetworkNode::SwitchToNewCalendarAndSpentChain(Calendar new_calendar, BitChain new_spent_chain)
{
    uint160 old_tip_message_hash = calendar.LastMinedCreditMessageHash();
    uint160 new_tip_message_hash = new_calendar.LastMinedCreditMessageHash();

    uint160 fork = credit_system->FindFork(old_tip_message_hash, new_tip_message_hash);

    LOCK(credit_message_handler->calendar_mutex);

    credit_system->RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(fork);
    credit_system->AddMinedCreditMessageAndPredecessorsToMainChain(new_tip_message_hash);
    calendar = new_calendar;
    spent_chain = new_spent_chain;

    if (wallet != NULL)
        wallet->SwitchAcrossFork(old_tip_message_hash, new_tip_message_hash, credit_system);
}

bool TeleportNetworkNode::StartCommunicator()
{
    if (config == NULL)
    {
        log_ << "Can't start communicator: no config\n";
        return false;
    }

    uint64_t port = config->Uint64("port");
    communicator = new Communicator("default_communicator", (unsigned short) port);
    communicator->main_node.SetTeleportNetworkNode(*this);
    AttachCommunicatorNetworkToHandlers();

    if (not communicator->StartListening())
    {
        log_ << "Failed to start communicator. Port " << port << " may already be in use\n";
        return false;
    }
    return true;
}

void TeleportNetworkNode::AttachCommunicatorNetworkToHandlers()
{
    credit_message_handler->SetNetwork(communicator->network);
    data_message_handler->SetNetwork(communicator->network);
    relay_message_handler->SetNetwork(communicator->network);
    deposit_message_handler->SetNetwork(communicator->network);
}

void TeleportNetworkNode::StopCommunicator()
{
    communicator->StopListening();
    delete communicator;
    credit_message_handler->network = NULL;
    data_message_handler->network = NULL;
    relay_message_handler->network = NULL;
    deposit_message_handler->network = NULL;
}

uint64_t TeleportNetworkNode::LatestBatchNumber()
{
    return Tip().mined_credit.network_state.batch_number;
}
