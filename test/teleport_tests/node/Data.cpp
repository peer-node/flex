#include "Data.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"
#include "test/teleport_tests/node/relays/structures/RelayMemoryCache.h"

#include "log.h"
#define LOG_CATEGORY "Data.cpp"

void Data::StoreRelayState(RelayState *relay_state)
{
    uint160 relay_state_hash = relay_state->GetHash160();
    msgdata[relay_state_hash]["relay_state"] = *relay_state;
    if (cache == NULL)
        CreateCache();
    cache->Store(*relay_state);
}

RelayState Data::GetRelayState(uint160 relay_state_hash)
{
    if (cache == NULL)
        CreateCache();
    if (cache->relay_lists.count(relay_state_hash))
        return cache->RetrieveRelayState(relay_state_hash);
    if (msgdata[relay_state_hash].HasProperty("relay_state"))
    {
        RelayState state = msgdata[relay_state_hash]["relay_state"];
        cache->Store(state);
        return state;
    }
    return RelayState();
}

void Data::CreateCache()
{
    cache = new RelayMemoryCache();
}

Data::~Data()
{
    if (using_internal_cache and cache != NULL)
    {
        delete cache;
        cache = NULL;
    }
}