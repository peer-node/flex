#include <src/vector_tools.h>
#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/relay_handler/RelayState.h"
#include "test/teleport_tests/node/relay_handler/RelayMemoryCache.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ARelayMemoryCache : public Test
{
public:
    Relay relay;
    RelayMemoryCache cache;
    MemoryDataStore keydata;
};

TEST_F(ARelayMemoryCache, StoresRelays)
{
    relay.number = 5;

    cache.StoreRelay(relay);

    uint160 relay_hash = relay.GetHash160();
    Relay &retrieved_relay = cache.Retrieve(relay_hash);

    ASSERT_THAT(retrieved_relay.number, Eq(relay.number));
}

TEST_F(ARelayMemoryCache, StoresAndRetrieves10000Relays)
{
    std::vector<uint160> hashes;

    for (uint32_t i = 0; i < 10000; i++)
    {
        relay.number = i;
        cache.StoreRelay(relay);
        hashes.push_back(relay.GetHash160());
    }

    for (uint32_t i = 0; i < 10000; i++)
    {
        relay = cache.Retrieve(hashes[i]);
        ASSERT_THAT(relay.number, Eq(i));
    }
}

TEST_F(ARelayMemoryCache, StoresAndRetrievesARelayStateWith10000Relays)
{
    RelayState state;

    for (uint64_t i = 0; i < 10000; i++)
    {
        Relay relay;
        relay.number = i;
        relay.hashes.join_message_hash = i;
        state.relays.push_back(relay);
    }

    cache.Store(state);
    uint160 state_hash = state.GetHash160();

    RelayState state2 = cache.RetrieveRelayState(state_hash);
    ASSERT_THAT(state2, Eq(state));
}

TEST_F(ARelayMemoryCache, StoresNoMoreThanTheMaximumNumberOfRelayStates)
{
    cache.maximum_number_of_relay_states = 100;

    RelayState state;
    for (uint64_t i = 0; i < 200; i++)
    {
        Relay relay;
        relay.number = i;
        state.relays.push_back(relay);
        cache.Store(state);
    }
    ASSERT_THAT(cache.relay_lists.size(), Eq(100));
}

TEST_F(ARelayMemoryCache, StoresNoMoreThanTheMinimumNumberOfRelaysNecessaryWhenStoringRelayStates)
{
    cache.maximum_number_of_relay_states = 100;

    RelayState state;
    for (uint64_t i = 0; i < 200; i++)
    {
        Relay relay;
        relay.number = i;
        state.relays.push_back(relay);
        if (i >= 50)
            EraseEntryFromVector(state.relays[0], state.relays);
        cache.Store(state);
    }
    // the 51 relays from 0 to 50 should be forgotten
    ASSERT_THAT(cache.relays.size(), Eq(149));
}


class ARelayMemoryCachePopulatedByMultipleThreads : public ARelayMemoryCache
{
public:
    std::vector<uint160> state_hashes;
    std::vector<RelayState> relay_states;

    virtual void SetUp()
    {
        RelayState state;

        for (uint32_t i = 0; i < 160; i++)
        {
            Relay relay;
            relay.number = i;
            relay.hashes.join_message_hash = i;
            state.relays.push_back(relay);
            state_hashes.push_back(state.GetHash160());
            relay_states.push_back(state);
        }

        for (uint32_t i = 0; i < 160; i++)
        {
            boost::thread thread(&ARelayMemoryCachePopulatedByMultipleThreads::StoreInRelayMemoryCache,
                                 this, relay_states[i]);
        }
        MilliSleep(5);
    }

    virtual void StoreInRelayMemoryCache(RelayState &state)
    {
        cache.Store(state);
    }
};

TEST_F(ARelayMemoryCachePopulatedByMultipleThreads, StoresTheDataCorrectly)
{
    for (uint32_t i = 0; i < 160; i ++)
    {
        RelayState retrieved_state = cache.RetrieveRelayState(state_hashes[i]);
        ASSERT_THAT(retrieved_state, Eq(relay_states[i])) << "failed at " << i << "\n";
    }
}