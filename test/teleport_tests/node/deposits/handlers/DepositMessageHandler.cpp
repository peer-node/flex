#include "DepositMessageHandler.h"

#include <utility>
#include "test/teleport_tests/node/TeleportNetworkNode.h"

using std::vector;
using std::pair;

extern std::vector<unsigned char> TCR;


#include "log.h"
#define LOG_CATEGORY "DepositMessageHandler.cpp"


void DepositMessageHandler::SetMinedCreditMessageBuilder(MinedCreditMessageBuilder *builder_)
{
    builder = builder_;
}

void DepositMessageHandler::SetNetworkNode(TeleportNetworkNode *teleport_network_node_)
{
    teleport_network_node = teleport_network_node_;
    address_request_handler.SetNetworkNode(teleport_network_node);
    address_disclosure_handler.SetNetworkNode(teleport_network_node);
    address_transfer_handler.SetNetworkNode(teleport_network_node);
    address_withdrawal_handler.SetNetworkNode(teleport_network_node);
}

void DepositMessageHandler::SetConfig(TeleportConfig& config_)
{
    config = config_;
}

void DepositMessageHandler::SetCalendar(Calendar& calendar_)
{
    calendar = &calendar_;
}

void DepositMessageHandler::SetSpentChain(BitChain& spent_chain_)
{
    spent_chain = &spent_chain_;
}

void DepositMessageHandler::HandleDepositAddressRequest(DepositAddressRequest request)
{
    address_request_handler.HandleDepositAddressRequest(request);
}

void DepositMessageHandler::CancelRequest(uint160 request_hash)
{
    log_ << "cancelling request_hash\n";
    data.depositdata[request_hash]["cancelled"] = true;
}

void DepositMessageHandler::HandleDepositAddressPartMessage(DepositAddressPartMessage part_message)
{
    address_request_handler.HandleDepositAddressPartMessage(part_message);
}

void DepositMessageHandler::HandleDepositAddressPartComplaint(DepositAddressPartComplaint complaint)
{
    address_request_handler.HandleDepositAddressPartComplaint(complaint);
}

void DepositMessageHandler::HandleNewTip(MinedCreditMessage &msg)
{
    address_request_handler.HandleEncodedRequests(msg);
    address_request_handler.HandlePostEncodedRequests(msg);
}

void DepositMessageHandler::HandleDepositAddressPartDisclosure(DepositAddressPartDisclosure disclosure)
{
    address_disclosure_handler.HandleDepositAddressPartDisclosure(disclosure);
}

void DepositMessageHandler::HandleDepositDisclosureComplaint(DepositDisclosureComplaint complaint)
{
    address_disclosure_handler.HandleDepositDisclosureComplaint(complaint);
}

void DepositMessageHandler::HandleDepositTransferMessage(DepositTransferMessage transfer)
{
    address_transfer_handler.HandleDepositTransferMessage(transfer);
}

void DepositMessageHandler::HandleTransferAcknowledgement(TransferAcknowledgement acknowledgement)
{
    address_transfer_handler.HandleTransferAcknowledgement(acknowledgement);
}

void DepositMessageHandler::HandleWithdrawalRequestMessage(WithdrawalRequestMessage request)
{
    address_withdrawal_handler.HandleWithdrawalRequestMessage(request);
}

void DepositMessageHandler::HandleWithdrawalMessage(WithdrawalMessage withdrawal_message)
{
    address_withdrawal_handler.HandleWithdrawalMessage(withdrawal_message);
}

void DepositMessageHandler::HandleWithdrawalComplaint(WithdrawalComplaint complaint)
{
    address_withdrawal_handler.HandleWithdrawalComplaint(complaint);
}

uint64_t DepositMessageHandler::GetRespondingRelay(Point deposit_address)
{
    vector<uint64_t> relay_numbers = GetRelaysForAddressPubkey(deposit_address);

    vector<pair<uint160, uint160> > disqualifications = data.depositdata[deposit_address]["disqualifications"];

    if (disqualifications.size() >= relay_numbers.size())
        return 0;

    return relay_numbers[disqualifications.size()];
}

vch_t DepositMessageHandler::GetCurrencyCode(Point deposit_address_pubkey)
{
    uint160 request_hash = data.depositdata[deposit_address_pubkey]["deposit_request"];
    DepositAddressRequest request = data.GetMessage(request_hash);
    vch_t code = request.currency_code;
    log_ << "currency code for deposit_address_pubkey is " << std::string(code.begin(), code.end()) << "\n";
    return code;
}

void DepositMessageHandler::AddAddress(Point address)
{
    return AddAddress(address, GetCurrencyCode(address));
}

void DepositMessageHandler::AddAddress(Point address_pubkey, vch_t currency)
{
    vector<Point> my_address_pubkeys = data.depositdata[currency]["my_address_public_keys"];

    log_ << "adding " << address_pubkey << " to " << my_address_pubkeys << "\n";
    if (not VectorContainsEntry(my_address_pubkeys, address_pubkey))
        my_address_pubkeys.push_back(address_pubkey);

    data.depositdata[currency]["my_address_public_keys"] = my_address_pubkeys;

    log_ << "result is " << my_address_pubkeys << "\n";

}

void DepositMessageHandler::AddDepositAddressesOwnedBySpecifiedPubKey(Point public_key)
{
    vector<Point> addresses_sent_to_public_key = DepositAddressPubkeysOwnedByRecipient(public_key);
    log_ << "adding deposit addresses: " << addresses_sent_to_public_key << "\n";
    for (auto new_address_pubkey : addresses_sent_to_public_key)
        AddAddress(new_address_pubkey);
}

void DepositMessageHandler::RemoveAddress(Point address)
{
    return RemoveAddress(address, GetCurrencyCode(address));
}

void DepositMessageHandler::RemoveAddress(Point address, vch_t currency)
{
    vector<Point> my_addresses = data.depositdata[currency]["my_address_public_keys"];

    log_ << "removing " << address << " from " << my_addresses << "\n";
    if (VectorContainsEntry(my_addresses, address))
        EraseEntryFromVector(address, my_addresses);

    data.depositdata[currency]["my_address_public_keys"] = my_addresses;

    log_ << "result is " << my_addresses << "\n";
}

void DepositMessageHandler::StoreRecipientOfDepositAddress(Point &recipient_pubkey, Point &deposit_address_pubkey)
{
    vector<Point> address_pubkeys = data.depositdata[recipient_pubkey]["address_pubkeys"];
    if (not VectorContainsEntry(address_pubkeys, deposit_address_pubkey))
        address_pubkeys.push_back(deposit_address_pubkey);
    data.depositdata[recipient_pubkey]["address_pubkeys"] = address_pubkeys;
}

void DepositMessageHandler::RemoveStoredRecipientOfDepositAddress(Point &old_recipient_pubkey, Point &deposit_address_pubkey)
{
    vector<Point> address_pubkeys = data.depositdata[old_recipient_pubkey]["address_pubkeys"];
    if (VectorContainsEntry(address_pubkeys, deposit_address_pubkey))
        EraseEntryFromVector(deposit_address_pubkey, address_pubkeys);
    data.depositdata[old_recipient_pubkey]["address_pubkeys"] = address_pubkeys;
}

void DepositMessageHandler::ChangeStoredRecipientOfDepositAddress(Point &old_recipient_pubkey,
                                                                  Point &deposit_address_pubkey,
                                                                  Point &new_recipient_pubkey)
{
    RemoveStoredRecipientOfDepositAddress(old_recipient_pubkey, deposit_address_pubkey);
    StoreRecipientOfDepositAddress(new_recipient_pubkey, deposit_address_pubkey);
}

vector<Point> DepositMessageHandler::DepositAddressPubkeysOwnedByRecipient(Point &recipient_pubkey)
{
    return data.depositdata[recipient_pubkey]["address_pubkeys"];
}

RelayState DepositMessageHandler::GetRelayStateOfEncodingMessage(Point address_pubkey)
{
    uint160 request_hash = data.depositdata[address_pubkey]["deposit_request"];
    uint160 encoded_request_identifier = data.depositdata[address_pubkey]["encoded_request_identifier"];
    uint160 encoding_message_hash = data.depositdata[encoded_request_identifier]["encoding_message_hash"];
    MinedCreditMessage msg = data.GetMessage(encoding_message_hash);
    return data.GetRelayState(msg.mined_credit.network_state.relay_state_hash);
}

std::vector<uint64_t> DepositMessageHandler::GetRelaysForAddressRequest(uint160 request_hash, uint160 encoding_message_hash)
{
    log_ << "GetRelaysForAddressRequest: encoding_message_hash is "
         << encoding_message_hash << " and request hash is " << request_hash << "\n";
    MinedCreditMessage msg = data.GetMessage(encoding_message_hash);
    uint160 relay_chooser = encoding_message_hash ^ request_hash;
    RelayState state = data.GetRelayState(msg.mined_credit.network_state.relay_state_hash);
    log_ << "Relay state retrieved is: " << state.GetHash160() << "\n";
    return state.ChooseRelaysForDepositAddressRequest(relay_chooser, PARTS_PER_DEPOSIT_ADDRESS);
}

std::vector<uint64_t> DepositMessageHandler::GetRelaysForAddressPubkey(Point address_pubkey)
{
    uint160 request_hash = data.depositdata[address_pubkey]["deposit_request"];
    uint160 encoded_request_identifier = data.depositdata[address_pubkey]["encoded_request_identifier"];
    uint160 encoding_credit_hash = data.depositdata[encoded_request_identifier]["encoding_message_hash"];
    return GetRelaysForAddressRequest(request_hash, encoding_credit_hash);
}

std::vector<Point> DepositMessageHandler::MyDepositAddressPublicKeys(std::string currency_code)
{
    log_ << "MyDepositAddressPublicKeys: " << currency_code << "\n";
    vch_t code(currency_code.begin(), currency_code.end());
    std::vector<Point> keys = data.depositdata[currency_code]["my_address_public_keys"];
    log_ << "found: " << keys << "\n";
    return data.depositdata[currency_code]["my_address_public_keys"];
}

std::vector<Point> DepositMessageHandler::MyDepositAddressPoints(std::string currency_code)
{
    std::vector<Point> deposit_address_points;
    auto public_keys = MyDepositAddressPublicKeys(currency_code);

    for (auto public_key : public_keys)
    {
        Point offset = data.depositdata[public_key]["offset_point"];
        deposit_address_points.push_back(public_key + offset);
    }
    return deposit_address_points;
}
