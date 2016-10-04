#include "InitialDataMessage.h"

InitialDataMessage::InitialDataMessage(InitialDataRequestMessage request, CreditSystem *credit_system)
{
    request_hash = request.GetHash160();
    PopulateMinedCreditMessages(request.latest_mined_credit_hash, credit_system);
    PopulateEnclosedData(request.latest_mined_credit_hash, credit_system);
    SetSpentChain(credit_system);
}

void InitialDataMessage::PopulateEnclosedData(uint160 msg_hash, CreditSystem *credit_system)
{
    message_data = EnclosedMessageData(msg_hash, credit_system);
}

void InitialDataMessage::SetSpentChain(CreditSystem* credit_system)
{
    Calend last_calend = GetLastCalend();
    spent_chain = credit_system->GetSpentChain(last_calend.GetHash160());
}

void InitialDataMessage::PopulateMinedCreditMessages(uint160 msg_hash, CreditSystem *credit_system)
{
    AddMinedCreditMessage(msg_hash, credit_system);
    msg_hash = credit_system->PreviousMinedCreditMessageHash(msg_hash);
    while (msg_hash != 0)
    {
        AddMinedCreditMessage(msg_hash, credit_system);
        if (credit_system->IsCalend(msg_hash))
            break;
        msg_hash = credit_system->PreviousMinedCreditMessageHash(msg_hash);
    }
    std::reverse(mined_credit_messages_in_current_diurn.begin(), mined_credit_messages_in_current_diurn.end());
}

void InitialDataMessage::AddMinedCreditMessage(uint160 msg_hash, CreditSystem *credit_system)
{
    MinedCreditMessage msg = credit_system->msgdata[msg_hash]["msg"];
    mined_credit_messages_in_current_diurn.push_back(msg);
}

Calend InitialDataMessage::GetLastCalend()
{
    if (mined_credit_messages_in_current_diurn.size() == 0)
        return Calend();
    return mined_credit_messages_in_current_diurn[0];
}