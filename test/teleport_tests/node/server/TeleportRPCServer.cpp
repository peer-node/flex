#include <src/base/util_hex.h>
#include <jsonrpccpp/client.h>
#include <test/teleport_tests/currency/CryptoCurrencyAddress.h>
#include <test/teleport_tests/currency/CryptoCurrencyPrivateKey.h>
#include "TeleportRPCServer.h"
#include "TeleportLocalServer.h"
#include "../TeleportNetworkNode.h"
#include "test/teleport_tests/node/historical_data/handlers/TipHandler.h"

#include "log.h"
#define LOG_CATEGORY "TeleportRPCServer.cpp"

using namespace jsonrpc;
using std::string;
using std::vector;

void TeleportRPCServer::SetTeleportLocalServer(TeleportLocalServer *teleport_local_server_)
{
    teleport_local_server = teleport_local_server_;
    node = teleport_local_server->teleport_network_node;
    data = &teleport_local_server->teleport_network_node->data;
}

void TeleportRPCServer::BindMethod(const char* method_name, void (TeleportRPCServer::*method)(const Json::Value &,Json::Value &))
{
    this->bindAndAddMethod(Procedure(method_name, PARAMS_BY_POSITION, JSON_STRING, NULL), method);
    methods.push_back(string(method_name));
}

void TeleportRPCServer::Help(const Json::Value& request, Json::Value& response)
{
    for (auto &method : methods)
        response.append(method);
}

void TeleportRPCServer::GetInfo(const Json::Value& request, Json::Value& response)
{
    response["network_id"] = network_id.ToString();
    if (teleport_local_server == NULL)
        return;
    response["balance"] = FormatMoney(teleport_local_server->Balance());

    uint160 latest_mined_credit_message_hash{0};
    auto latest_mined_credit_message = node->Tip();

    if (latest_mined_credit_message.mined_credit.network_state.batch_number != 0)
        latest_mined_credit_message_hash = latest_mined_credit_message.GetHash160();
    response["latest_mined_credit_message_hash"] = latest_mined_credit_message_hash.ToString();
    response["batch_number"] = latest_mined_credit_message.mined_credit.network_state.batch_number;

    if (node->relay_message_handler == NULL)
        return;

    RelayState &relay_state = node->relay_message_handler->relay_state;
    response["number_of_relays"] = (uint32_t)relay_state.relays.size();
}

void TeleportRPCServer::SetNetworkID(const Json::Value& request, Json::Value& response)
{
    auto network_id_string = request[0].asString();
    network_id = uint256(network_id_string);
    response = "ok";
}

void TeleportRPCServer::NewProof(const Json::Value &request, Json::Value &response)
{
    log_ << "processing new proof\n";
    teleport_local_server->HandleNewProof(NetworkSpecificProofOfWork(request["proof_base64"].asString()));
    log_ << "finished processing new proof\n";
}

void TeleportRPCServer::Balance(const Json::Value &request, Json::Value &response)
{
    response = FormatMoney((int64_t) teleport_local_server->Balance());
}

void TeleportRPCServer::StartMining(const Json::Value &request, Json::Value &response)
{
    teleport_local_server->StartMining();
}

void TeleportRPCServer::StartMiningAsynchronously(const Json::Value &request, Json::Value &response)
{
    teleport_local_server->StartMiningAsynchronously();
}

void TeleportRPCServer::KeepMiningAsynchronously(const Json::Value &request, Json::Value &response)
{
    teleport_local_server->KeepMiningAsynchronously();
}

int64_t GetAmountFromValue(const Json::Value &value)
{
    int64_t amount = 0;

    if (value.isString())
    {
        if (not ParseMoney(value.asString(), amount))
            throw jsonrpc::JsonRpcException(-32099, "bad amount");
    }
    else if (value.isDouble())
    {
        amount = (int64_t) (value.asDouble() * ONE_CREDIT);
    }
    else if (value.isInt())
    {
        amount = (int64_t) (value.asInt() * ONE_CREDIT);
    }
    return amount;
}

void TeleportRPCServer::SendToPublicKey(const Json::Value &request, Json::Value &response)
{
    std::string pubkey_hex = request[0].asString();
    Point public_key;
    if (not public_key.setvch(ParseHex(pubkey_hex)))
    {
        throw jsonrpc::JsonRpcException(-32099, "bad public key");
    }

    int64_t amount = GetAmountFromValue(request[1]);

    if (amount > teleport_local_server->Balance())
        throw jsonrpc::JsonRpcException(-32099, "insufficient balance");

    response = teleport_local_server->SendToPublicKey(public_key, amount).ToString();
}

void TeleportRPCServer::GetNewAddress(const Json::Value &request, Json::Value &response)
{
    Point public_key = teleport_local_server->teleport_network_node->GetNewPublicKey();
    response = teleport_local_server->teleport_network_node->GetTeleportAddressFromPublicKey(public_key);
}

void TeleportRPCServer::SendToAddress(const Json::Value &request, Json::Value &response)
{
    int64_t amount = GetAmountFromValue(request[1]);

    std::string address = request[0].asString();
    Point recipient_pubkey = teleport_local_server->teleport_network_node->GetPublicKeyFromTeleportAddress(address);

    if (recipient_pubkey.curve != SECP256K1 or recipient_pubkey == Point(SECP256K1, 0))
        throw jsonrpc::JsonRpcException(-32099, "bad address");

    if (amount > teleport_local_server->Balance())
        throw jsonrpc::JsonRpcException(-32099, "insufficient balance");
    log_ << "SendToAddress: address is " << address << "\n";
    uint160 tx_hash = teleport_local_server->teleport_network_node->SendToAddress(address, amount);
    response = tx_hash.ToString();
}

void TeleportRPCServer::AddNode(const Json::Value &request, Json::Value &response)
{
    std::string node_address = request[0].asString();
    CNode *peer = teleport_local_server->teleport_network_node->communicator->network.AddNode(node_address);

    if (peer == NULL)
        throw jsonrpc::JsonRpcException(-32099, "Failed to connect to " + node_address);
}

void TeleportRPCServer::RequestTips(const Json::Value &request, Json::Value &response)
{
    teleport_local_server->teleport_network_node->data_message_handler->tip_handler->RequestTips();
}

template <typename T> Json::Value GetJsonValue(T item)
{
    Json::Reader reader;
    Json::Value result;
    reader.parse(item.json(), result, false);
    return result;
}

void TeleportRPCServer::GetCalendar(const Json::Value &request, Json::Value &response)
{
    response = GetJsonValue(teleport_local_server->teleport_network_node->calendar);
}

void TeleportRPCServer::GetMinedCredit(const Json::Value &request, Json::Value &response)
{
    uint160 credit_hash(request[0].asString());
    MinedCreditMessage msg = data->creditdata[credit_hash]["msg"];
    response = GetJsonValue(msg.mined_credit);
}

void TeleportRPCServer::GetMinedCreditMessage(const Json::Value &request, Json::Value &response)
{
    uint160 credit_hash(request[0].asString());
    MinedCreditMessage msg = data->creditdata[credit_hash]["msg"];
    response = GetJsonValue(msg);
}

void TeleportRPCServer::ListUnspent(const Json::Value &request, Json::Value &response)
{
    std::vector<CreditInBatch> &credits = teleport_local_server->teleport_network_node->wallet->credits;

    Json::Value result;
    for (auto &credit : credits)
        result.append(GetJsonValue(credit));

    response = result;
}

void TeleportRPCServer::GetTransaction(const Json::Value &request, Json::Value &response)
{
    std::string tx_hash_string = request[0].asString();
    uint160 tx_hash(tx_hash_string);
    SignedTransaction tx = data->msgdata[tx_hash]["tx"];
    response = GetJsonValue(tx);
}

void TeleportRPCServer::GetBatch(const Json::Value &request, Json::Value &response)
{
    uint160 credit_hash(request[0].asString());
    MinedCreditMessage msg = data->creditdata[credit_hash]["msg"];
    auto batch = node->credit_system->ReconstructBatch(msg);
    response = GetJsonValue(batch);
}

void TeleportRPCServer::ListDepositAddresses(const Json::Value &request, Json::Value &response)
{
    string currency_code = request[0].asString();
    vector<Point> my_address_points = node->deposit_message_handler->MyDepositAddressPoints(currency_code);

    Json::Value result;

    for (auto &pubkey : my_address_points)
        result.append(node->GetCryptoCurrencyAddressFromPublicKey(currency_code, pubkey));

    response = result;
}

void TeleportRPCServer::RequestDepositAddress(const Json::Value &request, Json::Value &response)
{
    string currency_code = request[0].asString();
    auto address_request_handler = node->deposit_message_handler->address_request_handler;
    address_request_handler.SendDepositAddressRequest(currency_code);
}

void TeleportRPCServer::WithdrawDepositAddress(const Json::Value &request, Json::Value &response)
{
    string deposit_address = request[0].asString();
    if (not data->depositdata[deposit_address].HasProperty("deposit_address_point"))
        throw jsonrpc::JsonRpcException(-32099, "No such deposit address");

    Point deposit_address_point = data->depositdata[deposit_address]["deposit_address_point"];
    node->deposit_message_handler->address_withdrawal_handler.SendWithdrawalRequestMessage(deposit_address_point);
    log_ << "sent withdrawal request for " << deposit_address << "\n";
}

void TeleportRPCServer::TransferDepositAddress(const Json::Value &request, Json::Value &response)
{
    string deposit_address = request[0].asString();
    string recipient_teleport_address = request[1].asString();

    Point recipient_pubkey = node->GetPublicKeyFromTeleportAddress(recipient_teleport_address);

    Point deposit_address_point = data->depositdata[deposit_address]["deposit_address_point"];
    Point deposit_address_pubkey = data->depositdata[deposit_address_point]["address_pubkey"];

    node->deposit_message_handler->address_transfer_handler.SendDepositTransferMessage(deposit_address_pubkey,
                                                                                       recipient_pubkey);
    log_ << "sent transfer for " << deposit_address_pubkey << " to " << recipient_pubkey << "\n";
}

void TeleportRPCServer::GetKnownAddressBalance(const Json::Value &request, Json::Value &response)
{
    string address = request[0].asString();
    Point pubkey = node->GetPublicKeyFromTeleportAddress(address);
    uint64_t balance = node->GetKnownPubKeyBalance(pubkey);
    response = balance * (1.0 / ONE_CREDIT);
}

void TeleportRPCServer::ListWithdrawals(const Json::Value &request, Json::Value &response)
{
    string currency = request[0].asString();
    vector<Point> deposit_address_points = data->depositdata[currency]["withdrawals"];
    log_ << "withdrawals for " << currency << " are " << deposit_address_points << "\n";

    for (auto &deposit_address_point : deposit_address_points)
    {
        string deposit_address = CryptoCurrencyAddress(currency, deposit_address_point).ToString();
        CBigNum private_key = data->keydata[deposit_address_point]["privkey"];
        string currency_secret = CryptoCurrencyPrivateKey(currency, private_key).ToString();
        response[deposit_address] = currency_secret;
        log_ << deposit_address << " -> " << currency_secret << "\n";
    }
}

void TeleportRPCServer::GetCurrencySecretFromPrivateKey(const Json::Value &request, Json::Value &response)
{
    string currency = request[0].asString();
    string private_key_string = request[1].asString();

    CBigNum private_key;
    private_key.SetHex(private_key_string);

    response = CryptoCurrencyPrivateKey(currency, private_key).ToString();
}

void TeleportRPCServer::GetPrivateKeyFromCurrencySecret(const Json::Value &request, Json::Value &response)
{
    string currency_secret = request[0].asString();

    CBigNum private_key = CryptoCurrencyPrivateKey::CurrencySecretToPrivateKey(currency_secret);

    response = private_key.GetHex();
}

void TeleportRPCServer::GetCurrencyAddressFromPrivateKey(const Json::Value &request, Json::Value &response)
{
    string currency = request[0].asString();
    string private_key_string = request[1].asString();

    CBigNum private_key;
    private_key.SetHex(private_key_string);

    Point public_key(SECP256K1, private_key);

    response = CryptoCurrencyAddress(currency, public_key).ToString();
}

void TeleportRPCServer::StopMining(const Json::Value &request, Json::Value &response)
{
    teleport_local_server->keep_mining = false;
}

void TeleportRPCServer::GetUnspentCreditsFromStubs(const Json::Value &request, Json::Value &response)
{
    std::vector<uint32_t> stubs;
    for (auto stub : request)
        stubs.push_back((uint32_t)stub.asUInt64());

    auto credits = node->credit_system->tracker.GetCreditsPayingToRecipient(stubs, 0);

    for (auto credit : credits)
        if (not node->spent_chain.Get(credit.position))
            response.append(GetJsonValue(credit));
}

void TeleportRPCServer::ListCurrencies(const Json::Value &request, Json::Value &response)
{
    response.append("TCR");
    response.append("BTC");
}
