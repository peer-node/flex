#ifndef FLEX_NET_CONNECTIONS_H
#define FLEX_NET_CONNECTIONS_H

void ThreadOpenConnections();

void ThreadOpenAddedConnections();

bool OpenNetworkConnection(const CAddress& addrConnect,
                           CSemaphoreGrant *grantOutbound = NULL,
                           const char *strDest = NULL,
                           bool fOneShot = false);


#endif //FLEX_NET_CONNECTIONS_H
