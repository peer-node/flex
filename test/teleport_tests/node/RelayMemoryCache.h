#ifndef TELEPORT_RELAYDATASTORE_H
#define TELEPORT_RELAYDATASTORE_H


#include <src/crypto/uint256.h>
#include "Relay.h"

#define MAX_RELAY_STATES 8000


class RelayMemoryCache
{
public:
    std::map<uint160, Relay*> relays;
    std::vector<uint160> relay_hashes;
    std::map<uint160, uint32_t> relay_hash_positions;
    std::map<uint160, uint32_t> relay_hash_occurrences;
    std::vector<uint32_t> vacant_hash_positions;

    std::map<uint160, std::vector<uint32_t> > relay_lists;
    std::deque<uint160> relay_state_hashes;
    uint32_t maximum_number_of_relay_states{MAX_RELAY_STATES};

    Mutex mutex;

    ~RelayMemoryCache()
    {
        for (auto hash_and_relay : relays)
        {
            Relay *relay = hash_and_relay.second;
            delete relay;
        }
    }

    void StoreRelay(Relay &relay);

    Relay &Retrieve(uint160 &hash);

    void Store(RelayState &state);

    RelayState RetrieveRelayState(uint160 &relay_state_hash);

    void StoreRelay(Relay &relay, uint160 &relay_hash);

    void RemoveARelayList();

    void StoreRelayList(uint160 &relay_state_hash, std::vector<uint32_t> &relay_list);

    void IncrementRelayHashCount(uint160 &relay_hash);

    void DecrementRelayHashCounts(uint160 &relay_state_hash);

    void RemoveRelay(uint160 &relay_hash, uint32_t &relay_position);

    void AddRelayHash(uint160 &relay_hash);
};


#endif //TELEPORT_RELAYDATASTORE_H
