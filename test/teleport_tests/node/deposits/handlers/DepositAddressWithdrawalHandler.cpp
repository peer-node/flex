#include "DepositAddressWithdrawalHandler.h"
#include "test/teleport_tests/node/TeleportNetworkNode.h"

using std::vector;

#include "log.h"
#define LOG_CATEGORY "DepositAddressWithdrawalHandler"

DepositAddressWithdrawalHandler::DepositAddressWithdrawalHandler(DepositMessageHandler *deposit_message_handler):
        deposit_message_handler(deposit_message_handler), data(deposit_message_handler->data)
{
}

void DepositAddressWithdrawalHandler::SetNetworkNode(TeleportNetworkNode *node)
{
    teleport_network_node = node;
}

void DepositAddressWithdrawalHandler::SendWithdrawalRequestMessage(Point deposit_address_point)
{
    CBigNum private_recipient_key;
    private_recipient_key.Randomize(Secp256k1Point::Modulus());
    Point recipient_key(SECP256K1, private_recipient_key);
    data.keydata[recipient_key]["privkey"] = private_recipient_key;

    Point address_pubkey = data.depositdata[deposit_address_point]["address_pubkey"];
    WithdrawalRequestMessage request(address_pubkey, recipient_key, data);
    data.StoreMessage(request);
    deposit_message_handler->HandleWithdrawalRequestMessage(request);
    deposit_message_handler->Broadcast(request);
}

void DepositAddressWithdrawalHandler::HandleWithdrawalRequestMessage(WithdrawalRequestMessage message)
{
    log_ << "HandleWithdrawalRequestMessage for deposit address pubkey " << message.deposit_address_pubkey << "\n";
    if (not ValidateWithdrawalRequestMessage(message))
    {
        log_ << "failed validation\n";
        return;
    }
    AcceptWithdrawalRequestMessage(message);
}

bool DepositAddressWithdrawalHandler::ValidateWithdrawalRequestMessage(WithdrawalRequestMessage &message)
{
    //todo
    return true;
}

void DepositAddressWithdrawalHandler::AcceptWithdrawalRequestMessage(WithdrawalRequestMessage &message)
{
    auto relay_numbers = deposit_message_handler->GetRelaysForAddressPubkey(message.deposit_address_pubkey);
    auto relay_state = deposit_message_handler->GetRelayStateOfEncodingMessage(message.deposit_address_pubkey);

    auto request_hash = message.GetHash160();

    for (uint32_t position = 0; position < relay_numbers.size(); position++)
    {
        auto relay = relay_state.GetRelayByNumber(relay_numbers[position]);
        if (relay->PrivateKeyIsPresent(data))
        {
            SendWithdrawalMessage(request_hash, position);
        }
    }
}

void DepositAddressWithdrawalHandler::SendWithdrawalMessage(uint160 request_hash, uint32_t position)
{
    log_ << "sending withdrawal message for position " << position << "\n";
    WithdrawalMessage withdrawal_message(request_hash, position, data);
    data.StoreMessage(withdrawal_message);
    deposit_message_handler->HandleWithdrawalMessage(withdrawal_message);
    deposit_message_handler->Broadcast(withdrawal_message);
}

void DepositAddressWithdrawalHandler::HandleWithdrawalMessage(WithdrawalMessage withdrawal_message)
{
    log_ << "handling withdrawal message for position " << withdrawal_message.position << "\n";
    if (not ValidateWithdrawalMessage(withdrawal_message))
    {
        log_ << "failed validaition\n";
        return;
    }
    AcceptWithdrawalMessage(withdrawal_message);
}

bool DepositAddressWithdrawalHandler::ValidateWithdrawalMessage(WithdrawalMessage &withdrawal_message)
{
    //todo
    return true;
}

void DepositAddressWithdrawalHandler::AcceptWithdrawalMessage(WithdrawalMessage &withdrawal_message)
{
    if (not withdrawal_message.CheckAndSaveSecret(data))
    {
        //send complaint
        log_ << "check and save secret failed\n";
        return;
    }
    log_ << "saved address part secret for position " << withdrawal_message.position << "\n";
    Point deposit_address_pubkey = withdrawal_message.GetDepositAddressPubkey(data);
    uint32_t number_of_secrets_received = data.depositdata[deposit_address_pubkey]["secrets_received"];
    number_of_secrets_received++;
    data.depositdata[deposit_address_pubkey]["secrets_received"] = number_of_secrets_received;

    log_ << "number of secrets received for this address is now " << number_of_secrets_received << "\n";

    if (number_of_secrets_received == PARTS_PER_DEPOSIT_ADDRESS)
        HandleCompletedWithdrawal(deposit_address_pubkey);
}

void DepositAddressWithdrawalHandler::HandleCompletedWithdrawal(Point deposit_address_pubkey)
{
    CBigNum deposit_address_private_key = ReconstructDepositAddressPrivateKey(deposit_address_pubkey);
    Point offset_point = data.depositdata[deposit_address_pubkey]["offset_point"];
    CBigNum offset_secret = data.keydata[offset_point]["privkey"];
    auto private_key = deposit_address_private_key + offset_secret;
    auto deposit_address_point = deposit_address_pubkey + offset_point;

    data.keydata[deposit_address_point]["privkey"] = private_key;

    vch_t currency_code = data.depositdata[deposit_address_point]["currency_code"];
    std::string currency(currency_code.begin(), currency_code.end());

    log_ << "Importing private key " << private_key << " for address "
         << teleport_network_node->GetCryptoCurrencyAddressFromPublicKey(currency, deposit_address_point) << "\n";
    teleport_network_node->wallet->ImportPrivateKey(deposit_address_private_key + offset_secret);

    deposit_message_handler->RemoveAddress(deposit_address_pubkey, currency_code);

    if (currency == "TCR")
        deposit_message_handler->AddDepositAddressesOwnedBySpecifiedPubKey(deposit_address_point);
}

CBigNum DepositAddressWithdrawalHandler::ReconstructDepositAddressPrivateKey(Point address_pubkey)
{
    auto part_hashes = deposit_message_handler->address_request_handler.GetHashesOfAddressPartMessages(address_pubkey);
    CBigNum private_key{0};

    for (auto msg_hash : part_hashes)
    {
        DepositAddressPartMessage part_msg = data.GetMessage(msg_hash);
        CBigNum secret_part = data.keydata[part_msg.PubKey()]["privkey"];
        private_key += secret_part;
    }
    return private_key;
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
