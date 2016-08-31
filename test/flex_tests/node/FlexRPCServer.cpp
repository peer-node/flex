#include <src/base/util_hex.h>
#include <jsonrpccpp/client.h>
#include "FlexRPCServer.h"
#include "FlexLocalServer.h"

void FlexRPCServer::BindMethod(const char* method_name,
                               void (FlexRPCServer::*method)(const Json::Value &,Json::Value &))
{
    this->bindAndAddMethod(
            jsonrpc::Procedure(method_name, jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, NULL),
            method);
}

void FlexRPCServer::Help(const Json::Value& request, Json::Value& response)
{
    response = "help";
}

void FlexRPCServer::GetInfo(const Json::Value& request, Json::Value& response)
{
    response["network_id"] = network_id.ToString();
    if (flex_local_server == NULL)
        return;
    response["balance"] = FormatMoney(flex_local_server->Balance());

    uint160 latest_mined_credit_hash{0};
    auto latest_mined_credit = flex_local_server->flex_network_node->Tip().mined_credit;

    if (latest_mined_credit.network_state.batch_number != 0)
        latest_mined_credit_hash = latest_mined_credit.GetHash160();
    response["latest_mined_credit_hash"] = latest_mined_credit_hash.ToString();
    response["batch_number"] = latest_mined_credit.network_state.batch_number;
}

void FlexRPCServer::SetNetworkID(const Json::Value& request, Json::Value& response)
{
    auto network_id_string = request[0].asString();
    network_id = uint256(network_id_string);
    response = "ok";
}

void FlexRPCServer::NewProof(const Json::Value &request, Json::Value &response)
{
    flex_local_server->HandleNewProof(NetworkSpecificProofOfWork(request["proof_base64"].asString()));
}

void FlexRPCServer::Balance(const Json::Value &request, Json::Value &response)
{
    response = FormatMoney((int64_t) flex_local_server->Balance());
}

void FlexRPCServer::SetFlexLocalServer(FlexLocalServer *flex_local_server_)
{
    flex_local_server = flex_local_server_;
}

void FlexRPCServer::StartMining(const Json::Value &request, Json::Value &response)
{
    flex_local_server->StartMining();
}

void FlexRPCServer::StartMiningAsynchronously(const Json::Value &request, Json::Value &response)
{
    flex_local_server->StartMiningAsynchronously();
}

void FlexRPCServer::SetMegabytesUsed(const Json::Value &request, Json::Value &response)
{
    uint32_t number_of_megabytes = (uint32_t) request[0].asUInt64();
    flex_local_server->SetNumberOfMegabytesUsedForMining(number_of_megabytes);
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

void FlexRPCServer::SendToPublicKey(const Json::Value &request, Json::Value &response)
{
    std::string pubkey_hex = request[0].asString();
    Point public_key;
    if (not public_key.setvch(ParseHex(pubkey_hex)))
    {
        throw jsonrpc::JsonRpcException(-32099, "bad public key");
    }

    int64_t amount = GetAmountFromValue(request[1]);

    if (amount > flex_local_server->Balance())
        throw jsonrpc::JsonRpcException(-32099, "insufficient balance");

    response = flex_local_server->SendToPublicKey(public_key, amount).ToString();
}

void FlexRPCServer::GetNewAddress(const Json::Value &request, Json::Value &response)
{
    Point public_key = flex_local_server->flex_network_node->GetNewPublicKey();
    response = GetAddressFromPublicKey(public_key);
}

void FlexRPCServer::SendToAddress(const Json::Value &request, Json::Value &response)
{
    int64_t amount = GetAmountFromValue(request[1]);

    std::string address = request[0].asString();
    vch_t pubkey_hash_bytes;

    if (not DecodeBase58Check(address, pubkey_hash_bytes) or pubkey_hash_bytes.size() != 21)
        throw jsonrpc::JsonRpcException(-32099, "bad address");

    if (amount > flex_local_server->Balance())
        throw jsonrpc::JsonRpcException(-32099, "insufficient balance");

    uint160 tx_hash = flex_local_server->flex_network_node->SendToAddress(address, amount);
    response = tx_hash.ToString();
}

void FlexRPCServer::AddNode(const Json::Value &request, Json::Value &response)
{
    std::string node_address = request[0].asString();
    CNode *peer = flex_local_server->flex_network_node->communicator->network.AddNode(node_address);

    if (peer == NULL)
        throw jsonrpc::JsonRpcException(-32099, "Failed to connect to " + node_address);
}

void FlexRPCServer::RequestTips(const Json::Value &request, Json::Value &response)
{
    flex_local_server->flex_network_node->data_message_handler->RequestTips();
}

template <typename T> Json::Value GetJsonValue(T item)
{
    Json::Reader reader;
    Json::Value result;
    reader.parse(item.json(), result, false);
    return result;
}

void FlexRPCServer::GetCalendar(const Json::Value &request, Json::Value &response)
{
    response = GetJsonValue(flex_local_server->flex_network_node->calendar);
}

void FlexRPCServer::GetMinedCredit(const Json::Value &request, Json::Value &response)
{
    uint160 credit_hash(request[0].asString());
    MinedCredit mined_credit = flex_local_server->flex_network_node->creditdata[credit_hash]["mined_credit"];
    response = GetJsonValue(mined_credit);
}

void FlexRPCServer::ListUnspent(const Json::Value &request, Json::Value &response)
{
    std::vector<CreditInBatch> &credits = flex_local_server->flex_network_node->wallet->credits;

    Json::Value result;
    for (auto credit : credits)
        result.append(GetJsonValue(credit));

    response = result;
}

