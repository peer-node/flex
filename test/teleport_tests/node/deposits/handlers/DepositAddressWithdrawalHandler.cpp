#include "DepositAddressWithdrawalHandler.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"

DepositAddressWithdrawalHandler::DepositAddressWithdrawalHandler(DepositMessageHandler *deposit_message_handler):
        deposit_message_handler(deposit_message_handler), data(deposit_message_handler->data)
{
}

void DepositAddressWithdrawalHandler::SetNetworkNode(TeleportNetworkNode *node)
{
    teleport_network_node = node;
}

void DepositAddressWithdrawalHandler::HandleWithdrawalRequestMessage(WithdrawalRequestMessage message)
{

}

bool DepositAddressWithdrawalHandler::ValidateWithdrawalRequestMessage(WithdrawalRequestMessage &message)
{
    return false;
}

void DepositAddressWithdrawalHandler::AcceptWithdrawalRequestMessage(WithdrawalRequestMessage &message)
{

}

void DepositAddressWithdrawalHandler::HandleWithdrawalMessage(WithdrawalMessage message)
{

}

bool DepositAddressWithdrawalHandler::ValidateWithdrawalMessage(WithdrawalMessage &message)
{
    return false;
}

void DepositAddressWithdrawalHandler::AcceptWithdrawalMessage(WithdrawalMessage &message)
{

}

void DepositAddressWithdrawalHandler::HandleWithdrawalComplaint(WithdrawalComplaint complaint)
{

}

bool DepositAddressWithdrawalHandler::ValidateWithdrawalComplaint(WithdrawalComplaint &complaint)
{
    return false;
}

void DepositAddressWithdrawalHandler::AcceptWithdrawalComplaint(WithdrawalComplaint &complaint)
{

}
