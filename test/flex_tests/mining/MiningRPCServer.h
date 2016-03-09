#ifndef FLEX_MININGRPCSERVER_H
#define FLEX_MININGRPCSERVER_H

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include "FlexMiner.h"
#include "NetworkSpecificProofOfWork.h"

#define FLEX_MINER_VERSION "0.0.1"

class MiningRPCServer : public jsonrpc::AbstractServer<MiningRPCServer>
{
public:
    FlexMiner miner;

    MiningRPCServer(jsonrpc::HttpServer &server) :
            jsonrpc::AbstractServer<MiningRPCServer>(server)
    {
        BindMethod("version", &MiningRPCServer::SendVersion);
        BindMethod("list_seeds", &MiningRPCServer::ListSeeds);
        BindMethod("set_mining_information", &MiningRPCServer::SetMiningInformation);
        BindMethod("get_mining_root", &MiningRPCServer::GetMiningRoot);
        BindMethod("start_mining", &MiningRPCServer::StartMining);
        BindMethod("get_proof_of_work", &MiningRPCServer::GetProofOfWork);
        BindMethod("set_megabytes_used", &MiningRPCServer::SetMegabytesUsed);
        BindMethod("get_megabytes_used", &MiningRPCServer::GetMegabytesUsed);
    }

    void StartMining(const Json::Value& request, Json::Value& response)
    {
        miner.StartMining();
    }

    void SetMegabytesUsed(const Json::Value& request, Json::Value& response)
    {
        uint32_t megabytes_used = (uint32_t) request["megabytes_used"].asInt();
        miner.SetMemoryUsageInMegabytes(megabytes_used);
    }

    void GetMegabytesUsed(const Json::Value& request, Json::Value& response)
    {
        response = miner.megabytes_used;
    }

    void GetProofOfWork(const Json::Value& request, Json::Value& response)
    {
        uint256 network_id(request["network_id"].asString());
        auto proof = miner.GetProof();
        std::vector<uint256> branch = miner.BranchForNetwork(network_id);
        NetworkSpecificProofOfWork network_specific_proof(branch, proof);
        response["proof_base64"] = network_specific_proof.GetBase64String();
    }

    void GetMiningRoot(const Json::Value& request, Json::Value& response)
    {
        response = miner.MiningRoot().ToString();
    }

    void SendVersion(const Json::Value& request, Json::Value& response)
    {
        response = FLEX_MINER_VERSION;
    }

    void ListSeeds(const Json::Value& request, Json::Value& response)
    {
        response.resize(0);
        for (auto entry : miner.network_id_to_mining_info)
        {
            Json::Value json_entry;
            json_entry["network_id"] = entry.first.ToString();
            json_entry["network_seed"] = entry.second.network_seed.ToString();
            response.append(json_entry);
        }
    }

    void SetMiningInformation(const Json::Value& request, Json::Value& response)
    {
        auto network_id = request["network_id"];
        auto network_host = request["network_host"];
        auto network_port = request["network_port"];
        auto network_seed = request["network_seed"];
        auto network_difficulty = request["network_difficulty"];

        uint256 network_seed_, network_id_;
        network_seed_.SetHex(network_seed.asString());
        network_id_.SetHex(network_id.asString());

        NetworkMiningInfo info(network_id_, network_host.asString(),
                               network_port.asInt(), network_seed_,
                               network_difficulty.asInt64());
        miner.AddNetworkMiningInfo(info);
        response = "ok";
    }

    void BindMethod(const char* method_name,
                    void (MiningRPCServer::*method)(const Json::Value &,Json::Value &))
    {
        this->bindAndAddMethod(
                jsonrpc::Procedure(method_name, jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_STRING, NULL),
                method);
    }

};



#endif //FLEX_MININGRPCSERVER_H
