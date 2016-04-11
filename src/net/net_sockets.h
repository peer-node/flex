#ifndef FLEX_NET_SOCKETS_H
#define FLEX_NET_SOCKETS_H

extern std::vector<SOCKET> vhListenSocket;

void SocketSendData(CNode *pnode);
void ThreadSocketHandler();
bool BindListenPort(const CService &addrBind, string& strError);

#endif //FLEX_NET_SOCKETS_H
