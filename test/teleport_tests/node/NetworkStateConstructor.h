#ifndef TELEPORT_NETWORKSTATECONSTRUCTOR_H
#define TELEPORT_NETWORKSTATECONSTRUCTOR_H


#include <test/teleport_tests/mined_credits/EncodedNetworkState.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "TeleportConfig.h"

class NetworkStateConstructor
{
public:
    MemoryDataStore &msgdata, &creditdata;
    TeleportConfig *config{NULL};

    NetworkStateConstructor(MemoryDataStore &msgdata, MemoryDataStore &creditdata):
        msgdata(msgdata), creditdata(creditdata)
    { }

    void SetConfig(TeleportConfig &config_)
    {
        config = &config_;
    }

    EncodedNetworkState ConstructNetworkState();
};


#endif //TELEPORT_NETWORKSTATECONSTRUCTOR_H
