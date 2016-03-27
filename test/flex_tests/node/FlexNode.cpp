#include <openssl/rand.h>
#include <src/base/base58.h>
#include "FlexNode.h"

using jsonrpc::HttpAuthServer;

void FlexNode::LoadConfig(int argc, const char **argv)
{
    config_parser.ParseCommandLineArguments(argc, argv);
    config_parser.ReadConfigFile();
    config = config_parser.GetConfig();
}

void FlexNode::ThrowUsernamePasswordException()
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

void FlexNode::StartRPCServer()
{
    if (config["rpcuser"] == "" or config["rpcpassword"] == "")
        ThrowUsernamePasswordException();

    int port = (int) config.Value("rpcport", (uint64_t)8732);
    http_server = new HttpAuthServer(port, config["rpcuser"], config["rpcpassword"]);
    rpc_server = new FlexRPCServer(*http_server);
    rpc_server->StartListening();
}

void FlexNode::StopRPCServer()
{
    rpc_server->StopListening();
    delete rpc_server;
    delete http_server;
}





