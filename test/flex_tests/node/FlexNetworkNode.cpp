#include "FlexNetworkNode.h"

#include "log.h"
#define LOG_CATEGORY "FlexNetworkNode.cpp"

Calendar FlexNetworkNode::GetCalendar()
{
    return calendar;
}

uint64_t FlexNetworkNode::Balance()
{
    return wallet->Balance();
}

void FlexNetworkNode::HandleMessage(std::string channel, CDataStream stream, CNode *peer)
{
    if (channel == "credit")
        credit_message_handler->HandleMessage(stream, peer);

    if (channel == "data")
        data_message_handler->HandleMessage(stream, peer);
}

MinedCreditMessage FlexNetworkNode::Tip()
{
    return credit_message_handler->Tip();
}

MinedCreditMessage FlexNetworkNode::GenerateMinedCreditMessageWithoutProofOfWork()
{
    MinedCreditMessage msg = credit_message_handler->GenerateMinedCreditMessageWithoutProofOfWork();
    creditdata[msg.mined_credit.GetHash()]["generated_msg"] = msg;
    return msg;
}

MinedCreditMessage FlexNetworkNode::RecallGeneratedMinedCreditMessage(uint256 credit_hash)
{
    return creditdata[credit_hash]["generated_msg"];
}

void FlexNetworkNode::HandleNewProof(NetworkSpecificProofOfWork proof)
{
    uint256 credit_hash = proof.branch[0];
    auto msg = RecallGeneratedMinedCreditMessage(credit_hash);
    msg.proof_of_work = proof;
    credit_message_handler->HandleMinedCreditMessage(msg);
}

uint160 FlexNetworkNode::SendCreditsToPublicKey(Point public_key, uint64_t amount)
{
    SignedTransaction tx = wallet->GetSignedTransaction(public_key, amount);
    credit_message_handler->HandleSignedTransaction(tx);
    return tx.GetHash160();
}

Point FlexNetworkNode::GetNewPublicKey()
{
    return wallet->GetNewPublicKey();
}

uint160 FlexNetworkNode::SendToAddress(std::string address, int64_t amount)
{
    uint160 key_hash = GetKeyHashFromAddress(address);
    SignedTransaction tx = wallet->GetSignedTransaction(key_hash, (uint64_t) amount);
    credit_message_handler->HandleSignedTransaction(tx);
    return tx.GetHash160();
}

void FlexNetworkNode::SwitchToNewCalendarAndSpentChain(Calendar new_calendar, BitChain new_spent_chain)
{
    uint160 old_tip_credit_hash = calendar.LastMinedCreditHash();
    uint160 new_tip_credit_hash = new_calendar.LastMinedCreditHash();
    uint160 fork = credit_system->FindFork(calendar.LastMinedCreditHash(), new_tip_credit_hash);
    LOCK(credit_message_handler->calendar_mutex);
    credit_system->RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(fork);
    credit_system->AddCreditHashAndPredecessorsToMainChain(new_tip_credit_hash);
    calendar = new_calendar;
    spent_chain = new_spent_chain;
    if (wallet != NULL)
        wallet->SwitchAcrossFork(old_tip_credit_hash, new_tip_credit_hash, credit_system);
}

bool FlexNetworkNode::StartCommunicator()
{
    if (config == NULL)
    {
        log_ << "Can't start communicator: no config\n";
        return false;
    }
    uint64_t port = config->Uint64("port");
    communicator = new Communicator("default_communicator", (unsigned short) port);
    if (not communicator->StartListening())
    {
        log_ << "Failed to start communicator. Port " << port << " may already be in use\n";
        return false;
    }
    return true;
}

void FlexNetworkNode::StopCommunicator()
{
    communicator->StopListening();
    delete communicator;
}
