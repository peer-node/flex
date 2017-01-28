#include "Data.h"
#include "relay_handler/RelayState.h"
#include "relay_handler/RelayMemoryCache.h"


RelayState Data::GetRelayState(uint160 relay_state_hash)
{
    return RelayState();
}

void Data::CreateCache()
{
    cache = new RelayMemoryCache();
}

Data::~Data()
{
    if (cache != NULL)
    {
        delete cache;
        cache = NULL;
    }
}