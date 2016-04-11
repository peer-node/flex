#ifndef FLEX_NET_STARTSTOP_H
#define FLEX_NET_STARTSTOP_H

#include "net.h"

#define MAX_OUTBOUND_CONNECTIONS 8

bool BindListenPort(const CService &bindAddr, std::string& strError=REF(std::string()));
void StartNode(boost::thread_group& threadGroup);
bool StopNode();

#endif //FLEX_NET_STARTSTOP_H
