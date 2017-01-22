#include "RelayMemoryCache.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "RelayDataStore.cpp"


void RelayMemoryCache::StoreRelay(Relay &relay, uint160 &relay_hash)
{
    Relay *stored_relay = new Relay();
    *stored_relay = relay;
    relays[relay_hash] = stored_relay;
    if (relay_hash_positions.count(relay_hash) == 0)
        AddRelayHash(relay_hash);
}

void RelayMemoryCache::AddRelayHash(uint160 &relay_hash)
{
    if (vacant_hash_positions.size() == 0)
    {
        relay_hash_positions[relay_hash] = (uint32_t) relay_hashes.size();
        relay_hashes.push_back(relay_hash);
    }
    else
    {
        uint32_t position = vacant_hash_positions[0];
        relay_hashes[position] = relay_hash;
        relay_hash_positions[relay_hash] = position;
        EraseEntryFromVector(position, vacant_hash_positions);
    }
}

void RelayMemoryCache::StoreRelay(Relay &relay)
{
    uint160 relay_hash = relay.GetHash160();
    StoreRelay(relay, relay_hash);
}

Relay &RelayMemoryCache::Retrieve(uint160 &relay_hash)
{
    return *relays[relay_hash];
}

void RelayMemoryCache::Store(RelayState &state)
{
    uint160 relay_state_hash = state.GetHash160();

    LOCK(mutex);
    if (relay_lists.count(relay_state_hash)) return;

    std::vector<uint32_t> relay_list;
    for (auto &relay : state.relays)
    {
        uint160 relay_hash = relay.GetHash160();
        if (relay_hash_positions.count(relay_hash) == 0)
            StoreRelay(relay, relay_hash);
        IncrementRelayHashCount(relay_hash);
        relay_list.push_back(relay_hash_positions[relay_hash]);
    }
    StoreRelayList(relay_state_hash, relay_list);
}

void RelayMemoryCache::IncrementRelayHashCount(uint160 &relay_hash)
{
    if (not relay_hash_occurrences.count(relay_hash))
        relay_hash_occurrences[relay_hash] = 0;
    relay_hash_occurrences[relay_hash] += 1;
}

void RelayMemoryCache::StoreRelayList(uint160 &relay_state_hash, std::vector<uint32_t> &relay_list)
{
    relay_lists[relay_state_hash] = relay_list;
    relay_state_hashes.push_back(relay_state_hash);
    if (relay_lists.size() > maximum_number_of_relay_states)
        RemoveARelayList();
}

void RelayMemoryCache::RemoveARelayList()
{
    uint160 oldest_relay_state_hash = relay_state_hashes.front();
    relay_state_hashes.pop_front();
    DecrementRelayHashCounts(oldest_relay_state_hash);
    relay_lists.erase(oldest_relay_state_hash);
}

void RelayMemoryCache::DecrementRelayHashCounts(uint160 &relay_state_hash)
{
    std::vector<uint32_t> &relay_list = relay_lists[relay_state_hash];
    for (auto &relay_position : relay_list)
    {
        uint160 &relay_hash = relay_hashes[relay_position];
        relay_hash_occurrences[relay_hash] -= 1;
        if (relay_hash_occurrences[relay_hash] == 0)
            RemoveRelay(relay_hash, relay_position);
    }
}

void RelayMemoryCache::RemoveRelay(uint160 &relay_hash, uint32_t &relay_position)
{
    vacant_hash_positions.push_back(relay_position);
    relay_hash_positions.erase(relay_hash);
    relay_hash_occurrences.erase(relay_hash);
    delete relays[relay_hash];
    relays.erase(relay_hash);
}

RelayState RelayMemoryCache::RetrieveRelayState(uint160 &relay_state_hash)
{
    auto &relay_list = relay_lists[relay_state_hash];
    RelayState state;

    LOCK(mutex);
    for (auto &relay_hash_position : relay_list)
    {
        uint160 &relay_hash = relay_hashes[relay_hash_position];
        Relay relay = Retrieve(relay_hash);
        state.relays.push_back(relay);
    }
    return state;
}
