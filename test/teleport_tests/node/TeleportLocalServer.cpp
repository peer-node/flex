#include <openssl/rand.h>
#include <src/base/base58.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/client.h>
#include <test/teleport_tests/mining/NetworkSpecificProofOfWork.h>
#include "TeleportLocalServer.h"
#include "TeleportNetworkNode.h"

using jsonrpc::HttpAuthServer;

void TeleportLocalServer::LoadConfig(int argc, const char **argv)
{
    config_parser.ParseCommandLineArguments(argc, argv);
    config_parser.ReadConfigFile();
    config = config_parser.GetConfig();
}

std::string RandomPassword()
{
    unsigned char rand_pwd[32];
    RAND_bytes(rand_pwd, 32);
    return EncodeBase58(&rand_pwd[0], &rand_pwd[0]+32);
}

void TeleportLocalServer::ThrowUsernamePasswordException()
{
    std::string message = "To use teleportnet you must set the value of rpcpassword in the configuration file:\n"
                          + config_parser.ConfigFileName().string() + "\n"
                          "It is recommended you use the following random password:\n"
                          "rpcuser=teleportrpc\n"
                          "rpcpassword=" + RandomPassword() + "\n"
                          "(you do not need to remember this password)\n";
    throw(std::runtime_error(message));
}

bool TeleportLocalServer::StartRPCServer()
{
    if (config["rpcuser"] == "" or config["rpcpassword"] == "")
        ThrowUsernamePasswordException();

    uint64_t port = RPCPort();
    http_server = new HttpAuthServer((int) port, config["rpcuser"], config["rpcpassword"]);
    rpc_server = new TeleportRPCServer(*http_server);
    rpc_server->SetTeleportLocalServer(this);
    return rpc_server->StartListening();
}

void TeleportLocalServer::StopRPCServer()
{
    rpc_server->StopListening();
    delete rpc_server;
    delete http_server;
}

uint256 TeleportLocalServer::MiningSeed()
{
    if (teleport_network_node == NULL)
        return 0;
    MinedCreditMessage msg = teleport_network_node->GenerateMinedCreditMessageWithoutProofOfWork();
    return msg.mined_credit.GetHash();
}

std::string TeleportLocalServer::MiningAuthorization()
{
    return "Basic " + config.String("miningrpcuser") + ":" + config.String("miningrpcpassword");
}

Json::Value TeleportLocalServer::MakeRequestToMiningRPCServer(std::string method, Json::Value& request)
{
    jsonrpc::HttpClient http_client("http://localhost:" + config.String("miningrpcport", "8339"));
    http_client.AddHeader("Authorization", "Basic " + MiningAuthorization());
    jsonrpc::Client client(http_client);
    return client.CallMethod(method, request);
}

uint256 TeleportLocalServer::NetworkID()
{
    return uint256(config.String("network_id", "1"));
}

uint160 TeleportLocalServer::MiningDifficulty(uint256 mining_seed)
{
    if (teleport_network_node == NULL)
        return teleport_network_node->credit_system->initial_difficulty;
    auto msg = teleport_network_node->RecallGeneratedMinedCreditMessage(mining_seed);
    return msg.mined_credit.network_state.difficulty;
}

uint64_t TeleportLocalServer::RPCPort()
{
    return config.Uint64("rpcport", 8732);
}

std::string TeleportLocalServer::ExternalIPAddress()
{
    return "localhost";
}

void TeleportLocalServer::SetMiningNetworkInfo()
{
    Json::Value request;

    uint256 mining_seed = MiningSeed();
    request["network_id"] = NetworkID().ToString();
    request["network_port"] = (Json::Value::UInt64)RPCPort();
    request["network_seed"] = mining_seed.ToString();
    request["network_difficulty"] = MiningDifficulty(mining_seed).ToString();
    request["network_host"] = ExternalIPAddress();

    MakeRequestToMiningRPCServer("set_mining_information", request);
}

void TeleportLocalServer::StartMining()
{
    SetMiningNetworkInfo();
    Json::Value parameters;
    MakeRequestToMiningRPCServer("start_mining", parameters);
}

void TeleportLocalServer::StartMiningAsynchronously()
{
    SetMiningNetworkInfo();
    Json::Value parameters;
    MakeRequestToMiningRPCServer("start_mining_asynchronously", parameters);
}

void TeleportLocalServer::SetNumberOfMegabytesUsedForMining(uint32_t number_of_megabytes)
{
    Json::Value request;
    request["megabytes_used"] = number_of_megabytes;
    MakeRequestToMiningRPCServer("set_megabytes_used", request);
    if (teleport_network_node != NULL)
        teleport_network_node->credit_system->SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(number_of_megabytes);
}

NetworkSpecificProofOfWork TeleportLocalServer::GetLatestProofOfWork()
{
    return latest_proof_of_work;
}

void TeleportLocalServer::SetNetworkNode(TeleportNetworkNode *teleport_network_node_)
{
    teleport_network_node = teleport_network_node_;
    teleport_network_node->config = &config;
}

uint64_t TeleportLocalServer::Balance()
{
    if (teleport_network_node == NULL)
        return 0;
    return teleport_network_node->Balance();
}

void TeleportLocalServer::HandleNewProof(NetworkSpecificProofOfWork proof)
{
    latest_proof_of_work = proof;
    if (teleport_network_node != NULL)
        teleport_network_node->HandleNewProof(proof);
}

uint160 TeleportLocalServer::SendToPublicKey(Point public_key, int64_t amount)
{
    uint160 tx_hash = teleport_network_node->SendCreditsToPublicKey(public_key, (uint64_t) amount);
    return tx_hash;
}







