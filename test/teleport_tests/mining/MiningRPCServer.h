#ifndef TELEPORT_MININGRPCSERVER_H
#define TELEPORT_MININGRPCSERVER_H

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>
#include <test/teleport_tests/node/server/HttpAuthServer.h>

#include "TeleportMiner.h"
#include "NetworkSpecificProofOfWork.h"

#define TELEPORT_MINER_VERSION "0.0.1"


class MiningRPCServer : public jsonrpc::AbstractServer<MiningRPCServer>
{
public:
    TeleportMiner miner;

    MiningRPCServer(jsonrpc::HttpAuthServer& server) :
            jsonrpc::AbstractServer<MiningRPCServer>(server)
    {
        BindMethod("version", &MiningRPCServer::SendVersion);
        BindMethod("list_seeds", &MiningRPCServer::ListSeeds);
        BindMethod("set_mining_information", &MiningRPCServer::SetMiningInformation);
        BindMethod("get_mining_root", &MiningRPCServer::GetMiningRoot);
        BindMethod("start_mining", &MiningRPCServer::StartMining);
        BindMethod("start_mining_asynchronously", &MiningRPCServer::StartMiningASynchronously);
        BindMethod("get_proof_of_work", &MiningRPCServer::GetProofOfWork);
        BindMethod("set_megabytes_used", &MiningRPCServer::SetMegabytesUsed);
        BindMethod("get_megabytes_used", &MiningRPCServer::GetMegabytesUsed);
    }

    void StartMining(const Json::Value& request, Json::Value& response)
    {
        miner.StartMining();
    }

    void StartMiningASynchronously(const Json::Value& request, Json::Value& response)
    {
        boost::thread t(&TeleportMiner::StartMining, &miner);
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
        response = TELEPORT_MINER_VERSION;
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
        uint256 network_id(request["network_id"].asString());
        auto network_host = request["network_host"].asString();
        auto network_port = request["network_port"].asInt();
        uint256 network_seed(request["network_seed"].asString());
        uint160 network_difficulty(request["network_difficulty"].asString());


        NetworkMiningInfo info(network_id, network_host,
                               (uint32_t) network_port, network_seed,
                               network_difficulty);
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



#endif //TELEPORT_MININGRPCSERVER_H
