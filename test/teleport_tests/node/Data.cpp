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
    log_ << "GetRelayState: " << relay_state_hash << "\n";
    log_ << "using internal cache: " << using_internal_cache << "\n";
    log_ << "cache is null: " << (cache == NULL) << "\n";
    if (cache == NULL)
        CreateCache();
    log_ << "checking cache\n";
    if (cache == NULL)
    {
        CreateCache();
        log_ << "checking cache\n";
    }
    log_ << "cache size:\n";
    log_ << cache->relay_lists.size() << "\n";
    if (cache->relay_lists.count(relay_state_hash))
    {
        log_ << "retrieving from cache\n";
        return cache->RetrieveRelayState(relay_state_hash);
    }
    log_ << "checking data\n";
    if (msgdata[relay_state_hash].HasProperty("relay_state"))
    {
        log_ << "retrieving stored state\n";
        RelayState state = msgdata[relay_state_hash]["relay_state"];
        log_ << "caching state\n";
        cache->Store(state);
        log_ << "returning state\n";
        return state;
    }
    log_ << "returning empty state\n";
    return RelayState();
}

void Data::CreateCache()
{
    cache = new RelayMemoryCache();
    log_ << "created cache size:\n";
    log_ << cache->relay_lists.size() << "\n";
}

Data::~Data()
{
    if (using_internal_cache and cache != NULL)
    {
        delete cache;
        cache = NULL;
    }
}