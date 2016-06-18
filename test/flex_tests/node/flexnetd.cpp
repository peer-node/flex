#include <boost/program_options/detail/config_file.hpp>

#include "FlexLocalServer.h"


using namespace std;


void run_server(int argc, const char** argv)
{
    FlexLocalServer flex_local_server;
    FlexNetworkNode flex_network_node;

    flex_local_server.LoadConfig(argc, argv);
    flex_local_server.SetNetworkNode(&flex_network_node);

    if (flex_local_server.StartRPCServer())
    {
        cout << "Press enter to terminate."  << endl;
        getchar();
    }
    else
    {
        cout << "Failed to start server." << endl;
    }
}

int main(int argc, const char** argv)
{
    run_server(argc, argv);
}
