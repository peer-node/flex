#include <openssl/rand.h>
#include <src/base/base58.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/client.h>
#include <test/flex_tests/mining/NetworkSpecificProofOfWork.h>
#include "FlexLocalServer.h"

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

void FlexLocalServer::StartRPCServer()
{
    if (config["rpcuser"] == "" or config["rpcpassword"] == "")
        ThrowUsernamePasswordException();

    uint64_t port = RPCPort();
    http_server = new HttpAuthServer((int) port, config["rpcuser"], config["rpcpassword"]);
    rpc_server = new FlexRPCServer(*http_server);
    rpc_server->SetFlexNode(this);
    rpc_server->StartListening();
}

void FlexLocalServer::StopRPCServer()
{
    rpc_server->StopListening();
    delete rpc_server;
    delete http_server;
}

uint256 FlexLocalServer::MiningSeed()
{
    return uint256();
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

uint160 FlexLocalServer::MiningDifficulty()
{
    return 20;
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
    request["network_id"] = NetworkID().ToString();
    request["network_port"] = (Json::Value::UInt64)RPCPort();
    request["network_seed"] = MiningSeed().ToString();
    request["network_difficulty"] = MiningDifficulty().ToString();
    request["network_host"] = ExternalIPAddress();

    MakeRequestToMiningRPCServer("set_mining_information", request);
}

void FlexLocalServer::SetNumberOfMegabytesUsedForMining(uint32_t number_of_megabytes)
{
    Json::Value request;
    request["megabytes_used"] = number_of_megabytes;
    MakeRequestToMiningRPCServer("set_megabytes_used", request);
}

NetworkSpecificProofOfWork FlexLocalServer::GetLatestProofOfWork()
{
    return latest_proof_of_work;
}













