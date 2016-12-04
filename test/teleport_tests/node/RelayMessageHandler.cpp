
#include "RelayMessageHandler.h"

void RelayMessageHandler::SetCreditSystem(CreditSystem *credit_system_)
{
    credit_system = credit_system_;
}

void RelayMessageHandler::SetCalendar(Calendar *calendar_)
{
    calendar = calendar_;
}

void RelayMessageHandler::HandleRelayJoinMessage(RelayJoinMessage relay_join_message)
{

}

bool RelayMessageHandler::MinedCreditMessageHashIsInMainChain(RelayJoinMessage relay_join_message)
{
    return credit_system->IsInMainChain(relay_join_message.mined_credit_message_hash);
}
