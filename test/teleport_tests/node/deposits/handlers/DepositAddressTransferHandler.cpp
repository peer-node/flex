#include "DepositAddressTransferHandler.h"
#include "DepositMessageHandler.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"


DepositAddressTransferHandler::DepositAddressTransferHandler(DepositMessageHandler *deposit_message_handler):
        deposit_message_handler(deposit_message_handler), data(deposit_message_handler->data)
{
}

void DepositAddressTransferHandler::SetNetworkNode(TeleportNetworkNode *node)
{
    teleport_network_node = node;
}

void DepositAddressTransferHandler::HandleDepositTransferMessage(DepositTransferMessage message)
{

}

void DepositAddressTransferHandler::HandleTransferAcknowledgement(TransferAcknowledgement acknowledgement)
{

}
