#include <test/teleport_tests/currency/Currency.h>
#include "DepositAddressTransferHandler.h"
#include "DepositMessageHandler.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"

using std::string;
using std::vector;

#include "log.h"
#define LOG_CATEGORY "DepositAddressTransferHandler.cpp"

DepositAddressTransferHandler::DepositAddressTransferHandler(DepositMessageHandler *deposit_message_handler):
        deposit_message_handler(deposit_message_handler), data(deposit_message_handler->data)
{
}

void DepositAddressTransferHandler::SetNetworkNode(TeleportNetworkNode *node)
{
    teleport_network_node = node;
}

void DepositAddressTransferHandler::SendDepositTransferMessage(Point deposit_address_pubkey, Point recipient_pubkey)
{
    DepositTransferMessage transfer(deposit_address_pubkey, recipient_pubkey, data);
    transfer.Sign(data);
    data.StoreMessage(transfer);
    deposit_message_handler->Broadcast(transfer);
    deposit_message_handler->HandleDepositTransferMessage(transfer);
}

void DepositAddressTransferHandler::HandleDepositTransferMessage(DepositTransferMessage transfer)
{
    log_ << "HandleDepositTransferMessage: transfer " << transfer.deposit_address_pubkey << " to "
         << transfer.recipient_pubkey << "\n";
    if (not ValidateDepositTransferMessage(transfer))
    {
        log_ << "failed validation\n";
        return;
    }
    AcceptDepositTransferMessage(transfer);
    log_ << "broadcasting transfer: " << transfer.deposit_address_pubkey << " to "
         << transfer.recipient_pubkey << "\n";
    log_ << "with hash " << transfer.GetHash160() << "\n";
    deposit_message_handler->Broadcast(transfer);
}

bool DepositAddressTransferHandler::ValidateDepositTransferMessage(DepositTransferMessage &transfer)
{
    return transfer.VerifySignature(data);
}

void DepositAddressTransferHandler::AcceptDepositTransferMessage(DepositTransferMessage &transfer)
{
    log_ << "accepting transfer\n";
    Relay *responding_relay = GetRespondingRelay(transfer.deposit_address_pubkey);
    if (responding_relay != NULL and responding_relay->PrivateKeyIsPresent(data))
        SendTransferAcknowledgement(transfer);
}

Relay *DepositAddressTransferHandler::GetRespondingRelay(Point deposit_address_pubkey)
{
    std::vector<uint64_t> relay_numbers = deposit_message_handler->GetRelaysForAddressPubkey(deposit_address_pubkey);
    log_ << "relay numbers are: " << relay_numbers << "\n";
    auto &relay_state = teleport_network_node->relay_message_handler->relay_state;
    for (auto relay_number : relay_numbers)
    {
        auto relay = relay_state.GetRelayByNumber(relay_number);
        if (relay != NULL and relay->status == ALIVE)
            return relay;
        log_ << "relay: " << relay->number << " status: " << relay->status << "\n";
    }
    return NULL;
}

void DepositAddressTransferHandler::SendTransferAcknowledgement(DepositTransferMessage &transfer)
{
    log_ << "Sending acknowledgement for transfer of " << transfer.deposit_address_pubkey << "\n";
    TransferAcknowledgement acknowledgement(transfer.GetHash160(), data);
    acknowledgement.Sign(Data(data, &teleport_network_node->relay_message_handler->relay_state));
    data.StoreMessage(acknowledgement);
    deposit_message_handler->HandleTransferAcknowledgement(acknowledgement);
    deposit_message_handler->Broadcast(acknowledgement);
}

void DepositAddressTransferHandler::HandleTransferAcknowledgement(TransferAcknowledgement acknowledgement)
{
    Point deposit_address_pubkey = acknowledgement.GetDepositAddressPubkey(data);
    log_ << "handling acknowledgement for transfer of " << deposit_address_pubkey << "\n";
    if (not ValidateTransferAcknowledgement(acknowledgement))
    {
        log_ << "validation failed";
        return;
    }
    AcceptTransferAcknowledgement(acknowledgement);
}

bool DepositAddressTransferHandler::ValidateTransferAcknowledgement(TransferAcknowledgement &acknowledgement)
{
    return acknowledgement.VerifySignature(Data(data, &teleport_network_node->relay_message_handler->relay_state));
}

void DepositAddressTransferHandler::AcceptTransferAcknowledgement(TransferAcknowledgement &acknowledgement)
{
    // todo: check for conflict
    Point deposit_address_pubkey = acknowledgement.GetDepositAddressPubkey(data);
    log_ << "accepting acknowledgement for transfer of " << deposit_address_pubkey << "\n";
    data.depositdata[deposit_address_pubkey]["latest_transfer"] = acknowledgement.transfer_hash;

    DepositTransferMessage transfer = data.GetMessage(acknowledgement.transfer_hash);

    deposit_message_handler->ChangeStoredRecipientOfDepositAddress(transfer.sender_pubkey,
                                                                   transfer.deposit_address_pubkey,
                                                                   transfer.recipient_pubkey);

    if (data.keydata[transfer.sender_pubkey].HasProperty("privkey"))
    {
        log_ << "sent address with pubkey: " << deposit_address_pubkey << "\n";
        deposit_message_handler->RemoveAddress(deposit_address_pubkey);
    }
    if (data.keydata[transfer.recipient_pubkey].HasProperty("privkey"))
    {
        log_ << "received address with pubkey: " << deposit_address_pubkey << "\n";
        deposit_message_handler->AddAddress(deposit_address_pubkey);
        CBigNum offset = transfer.Offset(data);
        Point offset_point(offset);
        data.keydata[offset_point]["privkey"] = offset;
        data.depositdata[deposit_address_pubkey]["offset_point"] = offset_point;
        data.depositdata[deposit_address_pubkey + offset_point]["address_pubkey"] = deposit_address_pubkey;
        vch_t currency_code = transfer.GetDepositRequest(data).currency_code;
        string currency_code_string(currency_code.begin(), currency_code.end());
        deposit_message_handler->address_request_handler.RecordPointOfDepositAddress(deposit_address_pubkey + offset_point,
                                                                                     currency_code_string);

    }
}