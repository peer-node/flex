#ifndef FLEX_NETWORKSTATECONSTRUCTOR_H
#define FLEX_NETWORKSTATECONSTRUCTOR_H


#include <test/flex_tests/mined_credits/EncodedNetworkState.h>
#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include "FlexConfig.h"

class NetworkStateConstructor
{
public:
    MemoryDataStore &msgdata, &creditdata;
    FlexConfig *config{NULL};

    NetworkStateConstructor(MemoryDataStore &msgdata, MemoryDataStore &creditdata):
        msgdata(msgdata), creditdata(creditdata)
    { }

    void SetConfig(FlexConfig &config_)
    {
        config = &config_;
    }

    EncodedNetworkState ConstructNetworkState();
};


#endif //FLEX_NETWORKSTATECONSTRUCTOR_H
