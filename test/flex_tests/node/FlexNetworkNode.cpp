#include "FlexNetworkNode.h"


Calendar FlexNetworkNode::GetCalendar()
{
    return calendar;
}

double FlexNetworkNode::Balance()
{
    return wallet->Balance() * 1.0 / ONE_CREDIT;
}

void FlexNetworkNode::HandleMessage(std::string channel, CDataStream stream, CNode *peer)
{
    if (channel == "credit")
        credit_message_handler->HandleMessage(stream, peer);
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













