#include <openssl/rand.h>
#include <src/base/base58.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/client.h>
#include <test/flex_tests/mining/NetworkSpecificProofOfWork.h>
#include "FlexLocalServer.h"
#include "FlexNetworkNode.h"

using jsonrpc::HttpAuthServer;

void FlexLocalServer::LoadConfig(int argc, const char **argv)
{
    config_parser.ParseCommandLineArguments(argc, argv);
    config_parser.ReadConfigFile();
    config = config_parser.GetConfig();
}

void FlexLocalServer::ThrowUsernamePasswordException()
{
    unsigned char rand_pwd[32];
    RAND_bytes(rand_pwd, 32);
    std::string random_password = EncodeBase58(&rand_pwd[0], &rand_pwd[0]+32);
    std::string message = "To use Flex you must set the value of rpcpassword in the configuration file:\n"
                          + config_parser.ConfigFileName().string() + "\n"
                          "It is recommended you use the following random password:\n"
                          "rpcuser=flexrpc\n"
                          "rpcpassword=" + random_password + "\n"
                          "(you do not need to remember this password)\n";
    throw(std::runtime_error(message));
}

bool FlexLocalServer::StartRPCServer()
{
    if (config["rpcuser"] == "" or config["rpcpassword"] == "")
        ThrowUsernamePasswordException();

    uint64_t port = RPCPort();
    http_server = new HttpAuthServer((int) port, config["rpcuser"], config["rpcpassword"]);
    rpc_server = new FlexRPCServer(*http_server);
    rpc_server->SetFlexLocalServer(this);
    return rpc_server->StartListening();
}

void FlexLocalServer::StopRPCServer()
{
    rpc_server->StopListening();
    delete rpc_server;
    delete http_server;
}

uint256 FlexLocalServer::MiningSeed()
{
    if (flex_network_node == NULL)
        return 0;
    MinedCreditMessage msg = flex_network_node->GenerateMinedCreditMessageWithoutProofOfWork();
    return msg.mined_credit.GetHash();
}

std::string FlexLocalServer::MiningAuthorization()
{
    return "Basic " + config.String("miningrpcuser") + ":" + config.String("miningrpcpassword");
}

Json::Value FlexLocalServer::MakeRequestToMiningRPCServer(std::string method, Json::Value& request)
{
    jsonrpc::HttpClient http_client("http://localhost:" + config.String("miningrpcport", "8339"));
    http_client.AddHeader("Authorization", "Basic " + MiningAuthorization());
    jsonrpc::Client client(http_client);
    return client.CallMethod(method, request);
}

uint256 FlexLocalServer::NetworkID()
{
    return uint256(config.String("network_id", "1"));
}

uint160 FlexLocalServer::MiningDifficulty(uint256 mining_seed)
{
    if (flex_network_node == NULL)
        return INITIAL_DIFFICULTY;
    auto msg = flex_network_node->RecallGeneratedMinedCreditMessage(mining_seed);
    return msg.mined_credit.network_state.difficulty;
}

uint64_t FlexLocalServer::RPCPort()
{
    return config.Uint64("rpcport", 8732);
}

std::string FlexLocalServer::ExternalIPAddress()
{
    return "localhost";
}

void FlexLocalServer::SetMiningNetworkInfo()
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

void FlexLocalServer::StartMining()
{
    SetMiningNetworkInfo();
    Json::Value parameters;
    MakeRequestToMiningRPCServer("start_mining", parameters);
}

void FlexLocalServer::StartMiningAsynchronously()
{
    SetMiningNetworkInfo();
    Json::Value parameters;
    MakeRequestToMiningRPCServer("start_mining_asynchronously", parameters);
}

void FlexLocalServer::SetNumberOfMegabytesUsedForMining(uint32_t number_of_megabytes)
{
    Json::Value request;
    request["megabytes_used"] = number_of_megabytes;
    MakeRequestToMiningRPCServer("set_megabytes_used", request);
    if (flex_network_node != NULL)
        flex_network_node->credit_system->SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(number_of_megabytes);
}

NetworkSpecificProofOfWork FlexLocalServer::GetLatestProofOfWork()
{
    return latest_proof_of_work;
}

void FlexLocalServer::SetNetworkNode(FlexNetworkNode *flex_network_node_)
{
    flex_network_node = flex_network_node_;
}

double FlexLocalServer::Balance()
{
    if (flex_network_node == NULL)
        return 0;
    return flex_network_node->Balance();
}

void FlexLocalServer::HandleNewProof(NetworkSpecificProofOfWork proof)
{
    latest_proof_of_work = proof;
    if (flex_network_node != NULL)
        flex_network_node->HandleNewProof(proof);
}

uint160 FlexLocalServer::SendToPublicKey(Point public_key, int64_t amount)
{
    uint160 tx_hash = flex_network_node->SendCreditsToPublicKey(public_key, (uint64_t) amount);
    return tx_hash;
}







