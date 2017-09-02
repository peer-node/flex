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

void DepositMessageHandler::AddBatchToTip(MinedCreditMessage& msg)
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
    vector<uint64_t> relay_numbers = GetRelaysForAddress(deposit_address);

    vector<pair<uint160, uint160> > disqualifications = data.depositdata[deposit_address]["disqualifications"];

    if (disqualifications.size() >= relay_numbers.size())
        return 0;

    return relay_numbers[disqualifications.size()];
}

void DepositMessageHandler::AddAddress(Point address, vch_t currency)
{
    vector<Point> my_addresses = data.depositdata[currency]["addresses"];

    if (not VectorContainsEntry(my_addresses, address))
        my_addresses.push_back(address);

    data.depositdata[currency]["addresses"] = my_addresses;

    if (currency == TCR)
        data.keydata[KeyHash(address)]["watched"] = true;
}

void DepositMessageHandler::RemoveAddress(Point address, vch_t currency)
{
    vector<Point> my_addresses = data.depositdata[currency]["addresses"];

    if (VectorContainsEntry(my_addresses, address))
        EraseEntryFromVector(address, my_addresses);

    data.depositdata[currency]["addresses"] = my_addresses;
}

void DepositMessageHandler::AddAndRemoveMyAddresses(uint160 transfer_hash)
{
    log_ << "AddAndRemoveMyAddresses: " << transfer_hash << "\n";
    DepositTransferMessage transfer = msgdata[transfer_hash]["transfer"];
    DepositAddressRequest request = transfer.GetDepositRequest(data);
    vch_t currency = request.currency_code;
    log_ << "currency is " << currency << "\n";

    Point address = transfer.deposit_address;
    log_ << "address is " << address << "\n";

    Point sender_key = transfer.VerificationKey(data);
    log_ << "sender_key is " << sender_key << "\n";

    if (keydata[sender_key].HasProperty("privkey"))
    {
        log_ << "Sent by me - removing\n";
        RemoveAddress(address, currency);
    }

    log_ << "recipient_key_hash is " << transfer.recipient_key_hash << "\n";

    log_ << "have pubkey: " << keydata[transfer.recipient_key_hash].HasProperty("pubkey") << "\n";

    if (keydata[transfer.recipient_key_hash].HasProperty("pubkey"))
    {
        AddAddress(address, currency);
        uint160 address_hash = KeyHash(address);
        data.depositdata[address_hash]["address"] = address;
    }
}

std::vector<uint64_t> DepositMessageHandler::GetRelaysForAddressRequest(uint160 request_hash, uint160 encoding_credit_hash)
{
    log_ << "GetRelaysForAddressRequest: encoding_credit_hash is "
         << encoding_credit_hash << " and request hash is " << request_hash << "\n";
    if (encoding_credit_hash == 0)
        encoding_credit_hash = data.depositdata[request_hash]["encoding_credit_hash"];
    else
        data.depositdata[request_hash]["encoding_credit_hash"] = encoding_credit_hash;
    MinedCredit mined_credit = creditdata[encoding_credit_hash]["mined_credit"];
    log_ << "GetRelaysForAddressRequest: encoding mined is " << mined_credit;
    uint160 relay_chooser = mined_credit.GetHash160() ^ request_hash;
    RelayState state = data.GetRelayState(mined_credit.network_state.relay_state_hash);
    log_ << "Relay state retrieved is: " << state;
    return state.ChooseRelaysForDepositAddressRequest(relay_chooser, SECRETS_PER_DEPOSIT);
}

std::vector<uint64_t> DepositMessageHandler::GetRelaysForAddress(Point address)
{
    uint160 request_hash = data.depositdata[address]["deposit_request"];
    uint160 encoding_credit_hash;
    encoding_credit_hash = data.depositdata[request_hash]["encoding_credit_hash"];
    return GetRelaysForAddressRequest(request_hash, encoding_credit_hash);
}
