#include "net.h"
#include "net_cnode.h"


Network::~Network()
{
    // Close sockets
    for (auto pnode : vNodes)
        if (pnode->hSocket != INVALID_SOCKET)
            closesocket(pnode->hSocket);
    for (auto hListenSocket : vhListenSocket)
        if (hListenSocket != INVALID_SOCKET)
        if (closesocket(hListenSocket) == SOCKET_ERROR)
            LogPrintf("closesocket(hListenSocket) failed with error %s\n", NetworkErrorString(WSAGetLastError()));

    // clean up some globals (to help leak detection)
    for (auto pnode : vNodes)
        delete pnode;
    for (auto pnode : vNodesDisconnected)
        delete pnode;
    vNodes.clear();
    vNodesDisconnected.clear();
    delete semOutbound;
    semOutbound = NULL;
    delete pnodeLocalHost;
    pnodeLocalHost = NULL;

#ifdef WIN32
    // Shutdown Windows Sockets
        WSACleanup();
#endif
}