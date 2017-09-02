#include "DepositAddressRequestHandler.h"
#include "DepositMessageHandler.h"
#include <test/teleport_tests/node/credit/handlers/MinedCreditMessageBuilder.h>

using std::vector;

#include "log.h"
#define LOG_CATEGORY "DepositMessageHandler.cpp"

extern std::vector<unsigned char> TCR;


DepositAddressRequestHandler::DepositAddressRequestHandler(DepositMessageHandler *deposit_message_handler) :
    deposit_message_handler(deposit_message_handler), data(deposit_message_handler->data)
{
    teleport_network_node = deposit_message_handler->teleport_network_node;
}

void DepositAddressRequestHandler::HandleDepositAddressRequest(DepositAddressRequest request)
{
    uint160 request_hash = request.GetHash160();
    log_ << "HandleDepositAddressRequest: " << request_hash << "\n";
    if (teleport_network_node->Tip().mined_credit.network_state.batch_number == 0)
    {
        log_ << "not finished downloading\n";
        return;
    }
    if (data.depositdata[request_hash]["processed"])
    {
        log_ << "already processed\n";
        // should_forward = false;
        return;
    }
    if (not ValidateDepositAddressRequest(request))
    {
        log_ << "failed validation\n";
        // should_forward = false;
        return;
    }
    AcceptDepositAddressRequest(request);
}

bool DepositAddressRequestHandler::ValidateDepositAddressRequest(DepositAddressRequest &request)
{
    return request.VerifySignature(data) and request.CheckWork();
    // todo: ensure it's not an old request
}

void DepositAddressRequestHandler::AcceptDepositAddressRequest(DepositAddressRequest &request)
{
    auto request_hash = request.GetHash160();

    if (deposit_message_handler->mode == LIVE and deposit_message_handler->builder != NULL)
        deposit_message_handler->builder->AddToAcceptedMessages(request_hash);

    data.depositdata[request_hash]["processed"] = true;
}

void DepositAddressRequestHandler::HandleDepositAddressPartMessage(DepositAddressPartMessage part_message)
{
    log_ << "HandleDepositAddressPartMessage: " << part_message.GetHash160();

    if (not ValidateDepositAddressPartMessage(part_message))
    {
        // should_forward = false;
        return;
    }

    AcceptDepositAddressPartMessage(part_message);
}

bool DepositAddressRequestHandler::ValidateDepositAddressPartMessage(DepositAddressPartMessage &part_message)
{
    return part_message.VerifySignature(data);
}

void DepositAddressRequestHandler::AcceptDepositAddressPartMessage(DepositAddressPartMessage &part_message)
{
    uint160 part_msg_hash = part_message.GetHash160();
    uint160 request_hash = part_message.address_request_hash;

    data.depositdata[part_msg_hash]["processed"] = true;
}

void DepositAddressRequestHandler::HandleDepositAddressParts(uint160 request_hash, std::vector<uint160> part_hashes)
{
    log_ << "HandleDepositAddressParts: " << request_hash << "\n";
    DepositAddressRequest request;
    request = data.msgdata[request_hash]["deposit_request"];

    Point address = GetDepositAddressPubKey(request, part_hashes);

    data.depositdata[request_hash]["address"] = address;
    log_ << "address for " << request_hash << " is " << address << "\n";

    data.depositdata[address]["deposit_request"] = request_hash;
    log_ << "request_hash for " << address << " is " << request_hash << "\n";

    data.depositdata[address]["depositor_key"] = request.depositor_key;
    log_ << "depositor_key for " << address << " is " << request.depositor_key << "\n";

    if (data.depositdata[request_hash]["is_mine"])
        HandleMyDepositAddressParts(address, request, request_hash);
}

Point DepositAddressRequestHandler::GetDepositAddressPubKey(DepositAddressRequest request, vector<uint160> part_hashes)
{
    Point address(request.curve, 0);
    for (auto part_msg_hash : part_hashes)
    {
        DepositAddressPartMessage part_msg = data.msgdata[part_msg_hash]["deposit_part"];
        address += part_msg.PubKey();
    }
    return address;
}

void DepositAddressRequestHandler::HandleMyDepositAddressParts(Point address, DepositAddressRequest request,
                                                               uint160 request_hash)
{
    deposit_message_handler->AddAddress(address, request.currency_code);
    RecordPubKeyForDespositAddress(address);

    Point offset_point = data.depositdata[request_hash]["offset_point"];
    data.depositdata[address]["offset_point"] = offset_point;
    uint160 secret_address_hash = KeyHash(address + offset_point);
    if (request.currency_code == TCR)
        data.keydata[secret_address_hash]["watched"] = true;
}

void DepositAddressRequestHandler::RecordPubKeyForDespositAddress(Point address)
{
    uint160 address_hash = Hash160(address.getvch());
    data.depositdata[address_hash]["address"] = address;
    data.depositdata[KeyHash(address)]["address"] = address;
    data.depositdata[FullKeyHash(address)]["address"] = address;
    data.keydata[address_hash]["pubkey"] = address;
}

void DepositAddressRequestHandler::HandleEncodedRequests(MinedCreditMessage& msg)
{
    uint160 credit_hash = msg.mined_credit.GetHash160();
    log_ << "HandleEncodedRequests: " << credit_hash << "\n";
    msg.hash_list.RecoverFullHashes(data.msgdata);

    for (auto hash : msg.hash_list.full_hashes)
    {
        string_t type = data.msgdata[hash]["type"];

        if (type == "deposit_request")
        {
            log_ << "handling encoded request: " << hash << " of type " << type << "\n";
            HandleEncodedRequest(hash, credit_hash);
        }
    }
}

void DepositAddressRequestHandler::HandleEncodedRequest(uint160 request_hash, uint160 encoding_credit_hash)
{
    log_ << "HandleEncodedRequest: " << request_hash << " encoded in " << encoding_credit_hash << "\n";

    if (data.depositdata[encoding_credit_hash]["from_history"] or data.depositdata[encoding_credit_hash]["from_datamessage"])
        return;

    SendDepositAddressPartMessages(request_hash, encoding_credit_hash);

    ScheduleCheckForResponsesToRequest(request_hash, encoding_credit_hash);
}

void DepositAddressRequestHandler::SendDepositAddressPartMessages(uint160 request_hash, uint160 encoding_credit_hash)
{
    vector<uint64_t> relay_numbers = GetRelaysForAddressRequest(request_hash, encoding_credit_hash);

    for (uint32_t position = 0; position < relay_numbers.size(); position++)
    {
        if (RequestShouldBeRespondedTo(request_hash, encoding_credit_hash, relay_numbers, position))
            SendDepositAddressPartMessage(request_hash, encoding_credit_hash, position, relay_numbers);
    }
}

bool DepositAddressRequestHandler::RequestShouldBeRespondedTo(uint160 request_hash,
                                                              uint160 encoding_credit_hash,
                                                              vector<uint64_t> relay_numbers,
                                                              uint32_t position)
{
    if (RequestHasAlreadyBeenRespondedTo(request_hash, encoding_credit_hash, position))
        return false;

    uint64_t relay_number = relay_numbers[position];
    auto relay = teleport_network_node->relay_message_handler->relay_state.GetRelayByNumber(relay_number);
    return relay->PrivateKeyIsPresent(data);
}

bool DepositAddressRequestHandler::RequestHasAlreadyBeenRespondedTo(uint160 request_hash,
                                                                    uint160 encoding_credit_hash,
                                                                    uint32_t position)
{
    // todo
    return false;
}

void DepositAddressRequestHandler::SendDepositAddressPartMessage(uint160 request_hash,
                                                                 uint160 encoding_credit_hash,
                                                                 uint32_t position,
                                                                 vector<uint64_t> relay_numbers)
{
    uint64_t relay_number = relay_numbers[position];
    auto relay = teleport_network_node->relay_message_handler->relay_state.GetRelayByNumber(relay_number);

    DepositAddressPartMessage part_msg(request_hash, encoding_credit_hash, position, data, relay);
    deposit_message_handler->Broadcast(part_msg);
}

void DepositAddressRequestHandler::ScheduleCheckForResponsesToRequest(uint160 request_hash, uint160 encoding_credit_hash)
{
    if (data.depositdata[request_hash]["scheduled"])
        return;

    scheduler.Schedule("request_timeout_check", request_hash, GetTimeMicros() + RESPONSE_WAIT_TIME);
    data.depositdata[request_hash]["scheduled"] = true;
}

vector<uint64_t> DepositAddressRequestHandler::GetRelaysForAddressRequest(uint160 request_hash, uint160 encoding_credit_hash)
{
    log_ << "GetRelaysForAddressRequest: encoding_credit_hash is "
         << encoding_credit_hash << " and request hash is " << request_hash << "\n";
    if (encoding_credit_hash == 0)
        encoding_credit_hash = data.depositdata[request_hash]["encoding_credit_hash"];
    else
        data.depositdata[request_hash]["encoding_credit_hash"] = encoding_credit_hash;
    MinedCredit mined_credit = data.creditdata[encoding_credit_hash]["mined_credit"];
    uint160 relay_chooser = mined_credit.GetHash160() ^ request_hash;
    RelayState state = data.GetRelayState(mined_credit.network_state.relay_state_hash);
    return state.ChooseRelaysForDepositAddressRequest(relay_chooser, SECRETS_PER_DEPOSIT);
}

vector<uint64_t> DepositAddressRequestHandler::GetRelaysForAddress(Point address)
{
    uint160 request_hash = data.depositdata[address]["deposit_request"];
    uint160 encoding_credit_hash = data.depositdata[request_hash]["encoding_credit_hash"];
    return GetRelaysForAddressRequest(request_hash, encoding_credit_hash);
}

void DepositAddressRequestHandler::HandlePostEncodedRequests(MinedCreditMessage& msg)
{
    uint160 credit_hash = msg.mined_credit.GetHash160();
    log_ << "HandlePostEncodedRequests: " << credit_hash << "\n";

    uint160 previous_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
    MinedCreditMessage prev_msg = data.creditdata[previous_hash]["msg"];

    prev_msg.hash_list.RecoverFullHashes(data.msgdata);

    for (auto hash : prev_msg.hash_list.full_hashes)
    {
        string_t type = data.msgdata[hash]["type"];
        if (type == "deposit_request")
        {
            log_ << "post-encoded request: " << hash << "\n";
            HandlePostEncodedRequest(hash, credit_hash);
        }
    }
}

void DepositAddressRequestHandler::HandlePostEncodedRequest(uint160 request_hash, uint160 post_encoding_credit_hash)
{
    log_ << "HandlePostEncodedRequest: " << request_hash << "\n";
    DepositAddressPartMessage part_msg;
    vector<uint160> part_hashes = data.depositdata[request_hash]["parts"];

    for (auto part_msg_hash : part_hashes)
    {
        part_msg = data.msgdata[part_msg_hash]["deposit_part"];
        Point relay = part_msg.VerificationKey(data);

        if (data.keydata[relay].HasProperty("privkey"))
        {
            log_ << "have key for " << relay << "; sending disclosure\n";
            DepositAddressPartDisclosure disclosure(post_encoding_credit_hash, part_msg_hash, data);
            deposit_message_handler->Broadcast(disclosure);
            log_ << "sent disclosure\n";
        }
        scheduler.Schedule("disclosure_timeout_check", part_msg_hash, GetTimeMicros() + RESPONSE_WAIT_TIME);
    }
}

void DepositAddressRequestHandler::AddScheduledTasks()
{
    scheduler.AddTask(ScheduledTask("request_timeout_check", DoScheduledAddressRequestTimeoutCheck));
}

void DepositAddressRequestHandler::DoScheduledAddressRequestTimeoutCheck(uint160 request_hash)
{
    uint32_t parts_received = data.depositdata[request_hash]["parts_received"];
    Point address = data.depositdata[request_hash]["address"];

    uint160 encoding_message_hash = data.depositdata[request_hash]["encoding_message_hash"];
    DoSuccessionForNonRespondingRelays(request_hash, encoding_message_hash);

    if (not AllPartsHaveBeenReceived(address))
        scheduler.Schedule("request_timeout_check", request_hash, GetTimeMicros() + RESPONSE_WAIT_TIME);
}

bool DepositAddressRequestHandler::AllPartsHaveBeenReceived(uint160 request_hash, uint160 encoding_message_hash)
{
    for (uint32_t position = 0; position < SECRETS_PER_DEPOSIT; position++)
        if (not PartHasBeenReceived(request_hash, encoding_message_hash, position))
            return false;
    return true;
}

bool DepositAddressRequestHandler::PartHasBeenReceived(uint160 request_hash, uint160 encoding_message_hash, uint32_t position)
{
    uint160 encoded_request_identifier = request_hash + encoding_message_hash;
    uint32_t parts_received = data.depositdata[encoded_request_identifier]["parts_received"];
    return (bool)(parts_received & (1 << position));
}

void DepositAddressRequestHandler::RecordReceiptOfPart(uint160 request_hash, uint160 encoding_message_hash, uint32_t position)
{
    uint160 encoded_request_identifier = request_hash + encoding_message_hash;
    uint32_t parts_received = data.depositdata[encoded_request_identifier]["parts_received"];
    parts_received |= (1 << position);
    data.depositdata[encoded_request_identifier]["parts_received"] = parts_received;
}

void DepositAddressRequestHandler::DoSuccessionForNonRespondingRelays(uint160 request_hash, uint160 encoding_message_hash)
{
    // todo
}

