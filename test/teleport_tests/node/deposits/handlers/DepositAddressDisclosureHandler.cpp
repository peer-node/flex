#include "DepositAddressDisclosureHandler.h"
#include "DepositMessageHandler.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"

using std::vector;
using std::string;

#include "log.h"
#define LOG_CATEGORY "DepositAddressDisclosureHandler.cpp"


DepositAddressDisclosureHandler::DepositAddressDisclosureHandler(DepositMessageHandler *deposit_message_handler):
        deposit_message_handler(deposit_message_handler), data(deposit_message_handler->data)
{
}

void DepositAddressDisclosureHandler::SetNetworkNode(TeleportNetworkNode *node)
{
    teleport_network_node = node;
}

void DepositAddressDisclosureHandler::AddScheduledTasks()
{

}

void DepositAddressDisclosureHandler::HandleDepositAddressPartDisclosure(DepositAddressPartDisclosure disclosure)
{
    log_ << "received disclosure " << disclosure.Position(data) << " for encoded request "
         << disclosure.GetEncodedRequestIdentifier(data) << "\n";

    if (not ValidateDepositAddressPartDisclosure(disclosure))
        return;

    RecordDepositAddressPartDisclosure(disclosure);
    deposit_message_handler->Broadcast(disclosure);

    uint160 encoded_request_identifier = disclosure.GetEncodedRequestIdentifier(data);

    if (AllDisclosuresHaveBeenReceived(encoded_request_identifier))
        HandleReceiptOfAllDisclosures(encoded_request_identifier);
}

void DepositAddressDisclosureHandler::RecordDepositAddressPartDisclosure(DepositAddressPartDisclosure &disclosure)
{
    uint32_t position = disclosure.Position(data);
    uint160 encoded_request_identifier = disclosure.GetEncodedRequestIdentifier(data);

    uint32_t bitmap = data.depositdata[encoded_request_identifier]["disclosures"];
    bitmap |= (1 << position);
    data.depositdata[encoded_request_identifier]["disclosures"] = bitmap;
}

bool DepositAddressDisclosureHandler::AllDisclosuresHaveBeenReceived(uint160 encoded_request_identifier)
{
    uint32_t bitmap = data.depositdata[encoded_request_identifier]["disclosures"];

    for (uint32_t position = 0; position < PARTS_PER_DEPOSIT_ADDRESS; position++)
        if (not (bitmap & (1 << position)))
            return false;

    return true;
}

void DepositAddressDisclosureHandler::HandleReceiptOfAllDisclosures(uint160 encoded_request_identifier)
{
    log_ << "all disclosures received for " << encoded_request_identifier << "\n";

    if (data.depositdata[encoded_request_identifier]["is_mine"])
        RecordCompleteDisclosureOfMyRequest(encoded_request_identifier);
}

void DepositAddressDisclosureHandler::RecordCompleteDisclosureOfMyRequest(uint160 encoded_request_identifier)
{
    ActivateMyDepositAddress(encoded_request_identifier);
}

void DepositAddressDisclosureHandler::ActivateMyDepositAddress(uint160 encoded_request_identifier)
{
    uint160 request_hash = data.depositdata[encoded_request_identifier]["request_hash"];
    DepositAddressRequest request = data.GetMessage(request_hash);
    Point address_pubkey = data.depositdata[encoded_request_identifier]["address_pubkey"];
    deposit_message_handler->AddAddress(address_pubkey, request.currency_code);
}

bool DepositAddressDisclosureHandler::ValidateDepositAddressPartDisclosure(DepositAddressPartDisclosure &disclosure)
{
    return true;
}

void DepositAddressDisclosureHandler::HandleDepositDisclosureComplaint(DepositDisclosureComplaint complaint)
{

}
