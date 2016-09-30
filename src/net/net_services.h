#ifndef TELEPORT_NET_SERVICES_H
#define TELEPORT_NET_SERVICES_H


#include <src/base/protocol.h>
#include "netbase.h"

enum
{
    LOCAL_NONE,   // unknown
    LOCAL_IF,     // address a local interface listens on
    LOCAL_BIND,   // address explicit bound to
    LOCAL_UPNP,   // address reported by UPnP
    LOCAL_HTTP,   // address reported by whatismyip.com and similar
    LOCAL_MANUAL, // address explicitly specified (-externalip=)

    LOCAL_MAX
};


#endif //TELEPORT_NET_SERVICES_H
