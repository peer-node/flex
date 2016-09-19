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

}