#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/parsers.hpp>

#include "MiningRPCServer.h"

using namespace std;
using namespace jsonrpc;
using namespace boost::program_options;


variables_map GetCommandLineArguments(int argc, char** argv, options_description program_options)
{
    variables_map arguments;
    try
    {
        store(parse_command_line(argc, argv, program_options), arguments);
    }
    catch(boost::program_options::error& error)
    {
        cerr << "ERROR: " << error.what() << endl;
        cerr << program_options << endl;
    }
    return arguments;
}

void run_server(int port, string ssl_key_file, string ssl_cert_file)
{
    HttpAuthServer http_server(port, ssl_cert_file, ssl_key_file);
    MiningRPCServer mining_rpc_server(http_server);
    if (mining_rpc_server.StartListening())
    {
        cout << "Listening on port " << port << endl;
        cout << "Press enter to terminate."  << endl;
        getchar();
    }
    else
    {
        cout << "Failed to start server." << endl;
        if (ssl_cert_file != "" or ssl_key_file != "")
            cout << "Check your ssl key and certificate." << endl;
    }
}

int main(int argc, char** argv)
{
    int port;
    string ssl_key_file, ssl_cert_file;

    options_description program_options("Options");
    program_options.add_options()
      ("help,h", "Print help message")
      ("port,p", value<int>()->default_value(8339), "port to listen on")
      ("sslkey,k", value<string>(), "ssl key file")
      ("sslcert,c", value<string>(), "ssl certificate file");

    auto arguments = GetCommandLineArguments(argc, argv, program_options);

    if (arguments.count("port"))
        port = arguments["port"].as<int>();

    if (arguments.count("sslkey"))
        ssl_key_file = arguments["sslkey"].as<string>();

    if (arguments.count("sslcert"))
        ssl_cert_file = arguments["sslcert"].as<string>();

    if (arguments.count("help"))
    {
        cout << "Multi-TeleportNet Miner version " << TELEPORT_MINER_VERSION << endl << program_options << endl;
        return 0;
    }

    run_server(port, ssl_key_file, ssl_cert_file);
}