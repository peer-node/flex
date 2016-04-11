#ifndef FLEX_NET_EXTERNALIP_H
#define FLEX_NET_EXTERNALIP_H

#include "net.h"

bool GetMyExternalIP(CNetAddr& ipRet);
void ThreadGetMyExternalIP();

#endif //FLEX_NET_EXTERNALIP_H
