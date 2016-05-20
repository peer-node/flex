#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include "NetworkStateConstructor.h"


EncodedNetworkState NetworkStateConstructor::ConstructNetworkState()
{
    EncodedNetworkState network_state;
    if (config != NULL)
        network_state.network_id = config->Uint64("network_id", 0);
    return network_state;
}
