#include "DepositAddressRequestHandler.h"
#include "DepositMessageHandler.h"
#include <test/teleport_tests/node/credit/handlers/MinedCreditMessageBuilder.h>
#include <test/teleport_tests/currency/Currency.h>
#include <test/teleport_tests/node/relays/handlers/RelayMessageHandler.h>
#include "test/teleport_tests/node/TeleportNetworkNode.h"

using std::vector;
using std::string;

#include "log.h"
#define LOG_CATEGORY "DepositMessageHandler.cpp"

extern std::vector<unsigned char> TCR;


DepositAddressRequestHandler::DepositAddressRequestHandler(DepositMessageHandler *deposit_message_handler) :
    deposit_message_handler(deposit_message_handler), data(deposit_message_handler->data)
{
}

void DepositAddressRequestHandler::SetNetworkNode(TeleportNetworkNode *node)
{
    teleport_network_node = node;
}

void DepositAddressRequestHandler::AddScheduledTasks()
{
    scheduler.AddTask(ScheduledTask("request_timeout_check",
                                    &DepositAddressRequestHandler::DoScheduledAddressRequestTimeoutCheck, this));
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
    log_ << "broadcasting deposit address request " << request_hash << "\n";
    deposit_message_handler->Broadcast(request);
}

bool DepositAddressRequestHandler::ValidateDepositAddressRequest(DepositAddressRequest &request)
{
    return request.VerifySignature(data);// and request.CheckWork();
    // todo: ensure it's not an old request
}

void DepositAddressRequestHandler::AcceptDepositAddressRequest(DepositAddressRequest &request)
{
    auto request_hash = request.GetHash160();

    if (deposit_message_handler->mode == LIVE and deposit_message_handler->builder != NULL)
        deposit_message_handler->builder->AddToAcceptedMessages(request_hash);

    data.depositdata[request_hash]["processed"] = true;
}

void DepositAddressRequestHandler::HandleEncodedRequests(MinedCreditMessage& msg)
{
    uint160 encoding_message_hash = msg.GetHash160();
    log_ << "HandleEncodedRequests: " << encoding_message_hash << "\n";
    log_ << msg.hash_list.short_hashes.size() << " enclosed messages\n";
    msg.hash_list.RecoverFullHashes(data.msgdata);

    for (auto hash : msg.hash_list.full_hashes)
    {
        string_t type = data.msgdata[hash]["type"];
        log_ << "handling encoded message " << hash << " of type " << type << "\n";
        if (type == "deposit_request")
        {
            log_ << "handling encoded request: " << hash << " of type " << type << "\n";
            HandleEncodedRequest(hash, encoding_message_hash);
        }
    }
}

void DepositAddressRequestHandler::HandleEncodedRequest(uint160 request_hash, uint160 encoding_message_hash)
{
    log_ << "HandleEncodedRequest: " << request_hash << " encoded in " << encoding_message_hash << "\n";

    uint160 encoded_request_identifier = RecordEncodedRequest(request_hash, encoding_message_hash);

    if (data.depositdata[encoding_message_hash]["from_history"] or
            data.depositdata[encoding_message_hash]["from_datamessage"])
        return;

    ScheduleCheckForResponsesToRequest(encoded_request_identifier);
    SendDepositAddressPartMessages(request_hash, encoding_message_hash);
}

uint160 DepositAddressRequestHandler::RecordEncodedRequest(uint160 request_hash, uint160 encoding_message_hash)
{
    uint160 encoded_request_identifier = request_hash + encoding_message_hash;
    data.depositdata[encoded_request_identifier]["request_hash"] = request_hash;
    data.depositdata[encoded_request_identifier]["encoding_message_hash"] = encoding_message_hash;
    return encoded_request_identifier;
}

void DepositAddressRequestHandler::ScheduleCheckForResponsesToRequest(uint160 encoded_request_identifier)
{
    if (data.depositdata[encoded_request_identifier]["scheduled"])
        return;

    scheduler.Schedule("request_timeout_check", encoded_request_identifier, GetTimeMicros() + RESPONSE_WAIT_TIME);
    data.depositdata[encoded_request_identifier]["scheduled"] = true;
}

void DepositAddressRequestHandler::SendDepositAddressPartMessages(uint160 request_hash, uint160 encoding_message_hash)
{
    vector<uint64_t> relay_numbers = GetRelaysForAddressRequest(request_hash, encoding_message_hash);

    for (uint32_t position = 0; position < relay_numbers.size(); position++)
    {
        if (RequestShouldBeRespondedTo(request_hash, encoding_message_hash, relay_numbers, position))
        {
            log_ << "sending part message " << position << " for request " << request_hash << "\n";
            SendDepositAddressPartMessage(request_hash, encoding_message_hash, position, relay_numbers);
        }
    }
}

void DepositAddressRequestHandler::SendDepositAddressPartMessage(uint160 request_hash,
                                                                 uint160 encoding_message_hash,
                                                                 uint32_t position,
                                                                 vector<uint64_t> relay_numbers)
{
    uint64_t relay_number = relay_numbers[position];
    auto relay_state = RelayStateFromEncodingMessage(encoding_message_hash);
    auto relay = relay_state.GetRelayByNumber(relay_number);

    DepositAddressPartMessage part_msg(request_hash, encoding_message_hash, position, data, relay);
    part_msg.Sign(data);
    data.StoreMessage(part_msg);
    deposit_message_handler->Broadcast(part_msg);
    deposit_message_handler->HandleDepositAddressPartMessage(part_msg);
}

bool DepositAddressRequestHandler::RequestShouldBeRespondedTo(uint160 request_hash,
                                                              uint160 encoding_message_hash,
                                                              vector<uint64_t> relay_numbers,
                                                              uint32_t position)
{
    if (RequestHasAlreadyBeenRespondedTo(request_hash, encoding_message_hash, position))
        return false;

    uint64_t relay_number = relay_numbers[position];
    auto relay_state = RelayStateFromEncodingMessage(encoding_message_hash);
    auto relay = relay_state.GetRelayByNumber(relay_number);
    return relay->PrivateKeyIsPresent(data);
}

bool DepositAddressRequestHandler::RequestHasAlreadyBeenRespondedTo(uint160 request_hash,
                                                                    uint160 encoding_message_hash,
                                                                    uint32_t position)
{
    // todo
    return false;
}

RelayState DepositAddressRequestHandler::RelayStateFromEncodingMessage(uint160 encoding_message_hash)
{
    MinedCreditMessage msg = data.GetMessage(encoding_message_hash);
    return data.GetRelayState(msg.mined_credit.network_state.relay_state_hash);
}

vector<uint64_t> DepositAddressRequestHandler::GetRelaysForAddressRequest(uint160 request_hash,
                                                                          uint160 encoding_message_hash)
{
    uint160 relay_chooser = encoding_message_hash ^ request_hash;
    auto state = RelayStateFromEncodingMessage(encoding_message_hash);
    return state.ChooseRelaysForDepositAddressRequest(relay_chooser, PARTS_PER_DEPOSIT_ADDRESS);
}

vector<uint64_t> DepositAddressRequestHandler::GetRelaysForAddressPubkey(Point address_pubkey)
{
    uint160 request_hash = data.depositdata[address_pubkey]["deposit_request"];
    uint160 encoded_request_identifier = data.depositdata[address_pubkey]["encoded_request_identifier"];
    uint160 encoding_message_hash = data.depositdata[encoded_request_identifier]["encoding_message_hash"];
    return GetRelaysForAddressRequest(request_hash, encoding_message_hash);
}

void DepositAddressRequestHandler::DoScheduledAddressRequestTimeoutCheck(uint160 encoded_request_identifier)
{
    Point address_pubkey = data.depositdata[encoded_request_identifier]["address_pubkey"];

    uint160 encoding_message_hash = data.depositdata[encoded_request_identifier]["encoding_message_hash"];
    uint160 request_hash = data.depositdata[encoded_request_identifier]["request_hash"];

    DoSuccessionForNonRespondingRelays(request_hash, encoding_message_hash);

    if (not AllPartsHaveBeenReceived(request_hash, encoding_message_hash))
        scheduler.Schedule("request_timeout_check", encoded_request_identifier, GetTimeMicros() + RESPONSE_WAIT_TIME);
}

bool DepositAddressRequestHandler::AllPartsHaveBeenReceived(uint160 request_hash, uint160 encoding_message_hash)
{
    for (uint32_t position = 0; position < PARTS_PER_DEPOSIT_ADDRESS; position++)
        if (not PartHasBeenReceived(request_hash, encoding_message_hash, position))
            return false;
    return true;
}

bool DepositAddressRequestHandler::PartHasBeenReceived(uint160 request_hash, uint160 encoding_message_hash, uint32_t position)
{
    uint160 encoded_request_identifier = request_hash + encoding_message_hash;
    vector<uint160> part_hashes = data.depositdata[encoded_request_identifier]["parts"];
    return part_hashes[position] != 0;
}

void DepositAddressRequestHandler::DoSuccessionForNonRespondingRelays(uint160 request_hash, uint160 encoding_message_hash)
{
    // todo
}

void DepositAddressRequestHandler::HandleDepositAddressPartMessage(DepositAddressPartMessage part_message)
{
    log_ << "HandleDepositAddressPartMessage: " << part_message.GetHash160() << "\n";

    if (not ValidateDepositAddressPartMessage(part_message))
    {
        log_ << "failed validation\n";
        // should_forward = false;
        return;
    }

    AcceptDepositAddressPartMessage(part_message);
    deposit_message_handler->Broadcast(part_message);
}

bool DepositAddressRequestHandler::ValidateDepositAddressPartMessage(DepositAddressPartMessage &part_message)
{
    log_ << "ValidateDepositAddressPartMessage: ValidateEnclosedData: " << part_message.ValidateEnclosedData() << "\n";
    log_ << "ValidateDepositAddressPartMessage: VerifySignature: " << part_message.VerifySignature(data) << "\n";
    log_ << "depositaddresspartmessage is " << part_message.json() << "\n";
    return part_message.ValidateEnclosedData() and part_message.VerifySignature(data);
}

void DepositAddressRequestHandler::AcceptDepositAddressPartMessage(DepositAddressPartMessage &part_message)
{
    log_ << "AcceptDepositAddressPartMessage\n";
    uint160 part_msg_hash = part_message.GetHash160();
    log_ << "part_msg_hash = " << part_msg_hash << "\n";
    uint160 encoded_request_identifier = part_message.EncodedRequestIdentifier();
    log_ << "encoded_request_identifier = " << encoded_request_identifier << "\n";

    data.depositdata[part_msg_hash]["processed"] = true;
    log_ << part_msg_hash << " marked as processed\n";

    vector<uint160> part_hashes = data.depositdata[encoded_request_identifier]["parts"];
    part_hashes.resize(PARTS_PER_DEPOSIT_ADDRESS);
    part_hashes[part_message.position] = part_msg_hash;
    data.depositdata[encoded_request_identifier]["parts"] = part_hashes;

    log_ << "registered receipt of part\n";
    if (NumberOfReceivedMessageParts(part_hashes) == PARTS_PER_DEPOSIT_ADDRESS)
        HandleDepositAddressParts(encoded_request_identifier, part_message.address_request_hash, part_hashes);
}

uint32_t DepositAddressRequestHandler::NumberOfReceivedMessageParts(vector<uint160> part_hashes)
{
    uint32_t n = 0;
    for (auto part_hash : part_hashes)
        if (part_hash != 0)
            n++;
    return n;
}

vector<uint160> DepositAddressRequestHandler::GetHashesOfAddressPartMessages(Point address_pubkey)
{
    uint160 encoded_request_identifier = data.depositdata[address_pubkey]["encoded_request_identifier"];
    return data.depositdata[encoded_request_identifier]["parts"];
}

void DepositAddressRequestHandler::HandleDepositAddressParts(uint160 encoded_request_identifier,
                                                             uint160 request_hash,
                                                             vector<uint160> part_hashes)
{
    log_ << "HandleDepositAddressParts: " << request_hash << "\n";
    DepositAddressRequest request = data.GetMessage(request_hash);

    Point address_pubkey = GetDepositAddressPubKey(request, part_hashes);

    data.depositdata[address_pubkey]["encoded_request_identifier"] = encoded_request_identifier;
    data.depositdata[address_pubkey]["deposit_request"] = request_hash;
    data.depositdata[address_pubkey]["depositor_key"] = request.depositor_key;
    data.depositdata[address_pubkey]["relay_numbers"] = GetRelaysForAddressPubkey(address_pubkey);

    if (data.depositdata[request_hash]["is_mine"])
        HandleMyDepositAddressParts(address_pubkey, request, request_hash);
}

Point DepositAddressRequestHandler::GetDepositAddressPubKey(DepositAddressRequest request, vector<uint160> part_hashes)
{
    Point address_pubkey(request.curve, 0);
    for (auto part_msg_hash : part_hashes)
    {
        DepositAddressPartMessage part_msg = data.msgdata[part_msg_hash]["deposit_part"];
        address_pubkey += part_msg.PubKey();
    }
    return address_pubkey;
}

void DepositAddressRequestHandler::HandleMyDepositAddressParts(Point address_pubkey, DepositAddressRequest request,
                                                               uint160 request_hash)
{
    log_ << "received all parts for my deposit address: " << address_pubkey << "\n";
    deposit_message_handler->AddAddress(address_pubkey, request.currency_code);
    RecordPubKeyForDepositAddress(address_pubkey);

    Point offset = GenerateAndRecordOffsetForDepositAddress(address_pubkey);

    Point deposit_address_point = address_pubkey + offset;
    data.depositdata[deposit_address_point]["currency_code"] = request.currency_code;
    string currency_code(request.currency_code.begin(), request.currency_code.end());

    RecordPointOfDepositAddress(deposit_address_point, currency_code);

    if (request.currency_code == TCR)
        data.keydata[deposit_address_point]["watched"] = true;
}

void DepositAddressRequestHandler::RecordPointOfDepositAddress(Point deposit_address_point, string currency_code)
{
    string deposit_address = teleport_network_node->GetCryptoCurrencyAddressFromPublicKey(currency_code, deposit_address_point);
    data.depositdata[deposit_address]["deposit_address_point"] = deposit_address_point;
}

Point DepositAddressRequestHandler::GenerateAndRecordOffsetForDepositAddress(Point address_pubkey)
{
    CBigNum secret_offset;
    secret_offset.Randomize(address_pubkey.Modulus());
    Point offset_point(address_pubkey.curve, secret_offset);
    data.keydata[offset_point]["privkey"] = secret_offset;
    data.depositdata[address_pubkey]["offset_point"] = offset_point;
    data.depositdata[address_pubkey + offset_point]["address_pubkey"] = address_pubkey;
    return offset_point;
}

void DepositAddressRequestHandler::RecordPubKeyForDepositAddress(Point address_pubkey)
{
    uint160 address_pubkey_hash = Hash160(address_pubkey.getvch());
    data.depositdata[address_pubkey_hash]["address_pubkey"] = address_pubkey;
    data.depositdata[KeyHash(address_pubkey)]["address_pubkey"] = address_pubkey;
    data.depositdata[FullKeyHash(address_pubkey)]["address_pubkey"] = address_pubkey;
    data.keydata[address_pubkey_hash]["pubkey"] = address_pubkey;
}

void DepositAddressRequestHandler::HandlePostEncodedRequests(MinedCreditMessage& msg)
{
    uint160 postencoding_message_hash = msg.GetHash160();
    log_ << "HandlePostEncodedRequests: " << postencoding_message_hash << "\n";

    uint160 previous_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
    MinedCreditMessage prev_msg = data.creditdata[previous_hash]["msg"];

    prev_msg.hash_list.RecoverFullHashes(data.msgdata);

    for (auto hash : prev_msg.hash_list.full_hashes)
    {
        string_t type = data.msgdata[hash]["type"];
        if (type == "deposit_request")
        {
            log_ << "post-encoded request: " << hash << "\n";
            HandlePostEncodedRequest(hash, postencoding_message_hash);
        }
    }
}

void DepositAddressRequestHandler::HandlePostEncodedRequest(uint160 request_hash, uint160 post_encoding_message_hash)
{
    log_ << "HandlePostEncodedRequest: " << request_hash << "\n";
    MinedCreditMessage post_encoding_message = data.GetMessage(post_encoding_message_hash);

    uint160 encoding_message_hash = post_encoding_message.mined_credit.network_state.previous_mined_credit_message_hash;

    SendDepositAddressPartDisclosures(request_hash, encoding_message_hash, post_encoding_message_hash);
    uint160 encoded_request_identifier = request_hash + encoding_message_hash;
    ScheduleTimeoutChecksForPartDisclosures(encoded_request_identifier);
}

void DepositAddressRequestHandler::SendDepositAddressPartDisclosures(uint160 request_hash,
                                                                     uint160 encoding_message_hash,
                                                                     uint160 post_encoding_message_hash)
{
    RelayState relay_state = RelayStateFromEncodingMessage(encoding_message_hash);

    uint160 encoded_request_identifier = request_hash + encoding_message_hash;
    vector<uint160> part_hashes = data.depositdata[encoded_request_identifier]["parts"];

    for (auto part_msg_hash : part_hashes)
    {
        DepositAddressPartMessage part_message = data.GetMessage(part_msg_hash);
        Relay *relay = relay_state.GetRelayByNumber(part_message.RelayNumber(data));

        if (relay->PrivateKeyIsPresent(data))
            SendDepositAddressPartDisclosure(post_encoding_message_hash, part_msg_hash, data);
    }
}

void DepositAddressRequestHandler::ScheduleTimeoutChecksForPartDisclosures(uint160 encoded_request_identifier)
{
    vector<uint160> part_hashes = data.depositdata[encoded_request_identifier]["parts"];

    for (auto part_msg_hash : part_hashes)
        scheduler.Schedule("disclosure_timeout_check", part_msg_hash, GetTimeMicros() + RESPONSE_WAIT_TIME);
}

void DepositAddressRequestHandler::SendDepositAddressPartDisclosure(uint160 post_encoding_message_hash,
                                                                    uint160 part_msg_hash, Data data)
{
    log_ << "sending disclosure\n";
    DepositAddressPartDisclosure disclosure(post_encoding_message_hash, part_msg_hash, data);
    disclosure.Sign(data);
    data.StoreMessage(disclosure);
    deposit_message_handler->Broadcast(disclosure);
    deposit_message_handler->HandleDepositAddressPartDisclosure(disclosure);
    log_ << "sent disclosure\n";
}

void DepositAddressRequestHandler::SendDepositAddressRequest(std::string currency_code)
{
    uint8_t curve = SECP256K1;

    vch_t code(currency_code.begin(), currency_code.end());
    DepositAddressRequest request(curve, code, data);
    request.Sign(data);
    data.StoreMessage(request);
    data.depositdata[request.GetHash160()]["is_mine"] = true;
    deposit_message_handler->HandleDepositAddressRequest(request);
    deposit_message_handler->Broadcast(request);
}

void DepositAddressRequestHandler::HandleDepositAddressPartComplaint(DepositAddressPartComplaint complaint)
{

}

bool DepositAddressRequestHandler::ValidateDepositAddressPartComplaint(DepositAddressPartComplaint &complaint)
{
    return false;
}

void DepositAddressRequestHandler::AcceptDepositAddressPartComplaint(DepositAddressPartComplaint &complaint)
{

}
