#include <boost/program_options/detail/config_file.hpp>

#include "TeleportLocalServer.h"


using namespace std;


void run_server(int argc, const char** argv)
{
    TeleportLocalServer teleport_local_server;
    TeleportNetworkNode teleport_network_node;

    teleport_local_server.LoadConfig(argc, argv);
    teleport_local_server.SetNetworkNode(&teleport_network_node);
    teleport_local_server.teleport_network_node->StartCommunicator();

    if (teleport_local_server.StartRPCServer())
    {
        cout << "Press enter to terminate."  << endl;
        getchar();
    }
    else
    {
        cout << "Failed to start server." << endl;
    }
    teleport_local_server.teleport_network_node->StopCommunicator();
}

int main(int argc, const char** argv)
{
    run_server(argc, argv);
}