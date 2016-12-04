#include <src/crypto/secp256k1point.h>
#include <src/base/util_time.h>
#include "Relay.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "Relay.cpp"

RelayJoinMessage Relay::GenerateJoinMessage(MemoryDataStore &keydata, uint160 mined_credit_message_hash)
{
    RelayJoinMessage join_message;
    join_message.mined_credit_message_hash = mined_credit_message_hash;
    join_message.GenerateKeySixteenths(keydata);
    join_message.PopulatePrivateKeySixteenths(keydata);
    join_message.StorePrivateKey(keydata);
    return join_message;
}

KeyDistributionMessage Relay::GenerateKeyDistributionMessage(Databases data, uint160 encoding_message_hash, RelayState &relay_state)
{
    KeyDistributionMessage key_distribution_message;
    key_distribution_message.encoding_message_hash = encoding_message_hash;
    key_distribution_message.relay_join_hash = join_message_hash;
    key_distribution_message.relay_number = number;
    key_distribution_message.PopulateKeySixteenthsEncryptedForKeyQuarterHolders(data.keydata, *this, relay_state);
    key_distribution_message.PopulateKeySixteenthsEncryptedForFirstSetOfKeySixteenthHolders(data.keydata, *this,
                                                                                            relay_state);
    return key_distribution_message;
}

std::vector<std::vector<uint64_t> > Relay::KeyPartHolderGroups()
{
    return std::vector<std::vector<uint64_t> >{quarter_key_holders,
                                               first_set_of_key_sixteenth_holders,
                                               second_set_of_key_sixteenth_holders};
}

std::vector<CBigNum> Relay::PrivateKeySixteenths(MemoryDataStore &keydata)
{
    std::vector<CBigNum> private_key_sixteenths;

    for (auto public_key_sixteenth : public_key_sixteenths)
    {
        CBigNum private_key_sixteenth = keydata[public_key_sixteenth]["privkey"];
        private_key_sixteenths.push_back(private_key_sixteenth);
    }
    return private_key_sixteenths;
}
