#include "KeyDistributionMessage.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "KeyDistributionMessage.cpp"

CBigNum EncryptSecretForPublicKey(CBigNum secret, Point public_key)
{
    CBigNum shared_secret = Hash(secret * public_key);
    return secret ^ shared_secret;
}

void KeyDistributionMessage::PopulateKeySixteenthsEncryptedForKeyQuarterHolders(MemoryDataStore &keydata,
                                                                                Relay &relay,
                                                                                RelayState &relay_state)
{
    std::vector<CBigNum> private_key_sixteenths = relay.PrivateKeySixteenths(keydata);
    std::vector<Point> recipient_public_keys = relay_state.GetKeyQuarterHoldersAsListOf16Recipients(relay.number);

    for (uint64_t i = 0; i < private_key_sixteenths.size(); i++)
        key_sixteenths_encrypted_for_key_quarter_holders.push_back(EncryptSecretForPublicKey(private_key_sixteenths[i],
                                                                                             recipient_public_keys[i]));
}

void KeyDistributionMessage::PopulateKeySixteenthsEncryptedForKeySixteenthHolders(MemoryDataStore &keydata,
                                                                                  Relay &relay,
                                                                                  RelayState &relay_state)
{
    auto private_key_sixteenths = relay.PrivateKeySixteenths(keydata);
    auto first_set_of_recipient_public_keys = relay_state.GetKeySixteenthHoldersAsListOf16Recipients(relay.number, 1);
    auto second_set_of_recipient_public_keys = relay_state.GetKeySixteenthHoldersAsListOf16Recipients(relay.number, 2);

    for (uint64_t i = 0; i < private_key_sixteenths.size(); i++)
    {
        key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders.push_back(
                EncryptSecretForPublicKey(private_key_sixteenths[i], first_set_of_recipient_public_keys[i]));
        key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders.push_back(
                EncryptSecretForPublicKey(private_key_sixteenths[i], second_set_of_recipient_public_keys[i]));
    }
}

bool KeyDistributionMessage::KeyQuarterHolderPrivateKeyIsAvailable(uint64_t position, Data data,
                                                                   RelayState &relay_state, Relay &relay)
{
    auto recipient_public_keys = relay_state.GetKeyQuarterHoldersAsListOf16Recipients(relay.number);
    return data.keydata[recipient_public_keys[position]].HasProperty("privkey");
}

bool KeyDistributionMessage::KeySixteenthForKeyQuarterHolderIsCorrectlyEncrypted(uint64_t position, Data data,
                                                                                 RelayState &relay_state, Relay &relay)
{
    auto recipient_public_keys = relay_state.GetKeyQuarterHoldersAsListOf16Recipients(relay.number);
    CBigNum private_key_sixteenth = data.keydata[relay.public_key_sixteenths[position]]["privkey"];
    CBigNum encrypted_private_key_sixteenth = EncryptSecretForPublicKey(private_key_sixteenth,
                                                                        recipient_public_keys[position]);
    return key_sixteenths_encrypted_for_key_quarter_holders[position] == encrypted_private_key_sixteenth;
}

bool KeyDistributionMessage::KeySixteenthHolderPrivateKeyIsAvailable(uint32_t position, uint32_t first_or_second_set,
                                                                     Data data, RelayState &relay_state,
                                                                     Relay &relay)
{
    auto recipient_pubkeys = relay_state.GetKeySixteenthHoldersAsListOf16Recipients(relay.number, first_or_second_set);
    return data.keydata[recipient_pubkeys[position]].HasProperty("privkey");
}

bool KeyDistributionMessage::KeySixteenthForKeySixteenthHolderIsCorrectlyEncrypted(uint64_t position,
                                                                                   uint32_t first_or_second_set,
                                                                                   Data data, RelayState &state,
                                                                                   Relay &relay)
{
    auto recipient_pubkey = state.GetKeySixteenthHoldersAsListOf16Recipients(relay.number, first_or_second_set)[position];
    CBigNum recipient_privkey = data.keydata[recipient_pubkey]["privkey"];
    CBigNum shared_secret = Hash(recipient_privkey * relay.public_key_sixteenths[position]);
    CBigNum encrypted_key_sixteenth = first_or_second_set == 1
                                      ? key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[position]
                                      : key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[position];
    CBigNum private_key_sixteenth = shared_secret ^ encrypted_key_sixteenth;
    if (Point(SECP256K1, private_key_sixteenth) != relay.public_key_sixteenths[position])
        return false;
    data.keydata[relay.public_key_sixteenths[position]]["privkey"] = private_key_sixteenth;
    return true;
}

bool KeyDistributionMessage::TryToRecoverKeySixteenthEncryptedToQuarterKeyHolder(uint32_t position, Data data,
                                                                                 RelayState &state, Relay &relay)
{
    auto recipient_pubkey = state.GetKeyQuarterHoldersAsListOf16Recipients(relay.number)[position];
    CBigNum recipient_privkey = data.keydata[recipient_pubkey]["privkey"];
    CBigNum shared_secret = Hash(recipient_privkey * relay.public_key_sixteenths[position]);
    CBigNum private_key_sixteenth = shared_secret ^ key_sixteenths_encrypted_for_key_quarter_holders[position];
    if (Point(SECP256K1, private_key_sixteenth) != relay.public_key_sixteenths[position])
        return false;
    data.keydata[relay.public_key_sixteenths[position]]["privkey"] = private_key_sixteenth;
    return true;
}

bool KeyDistributionMessage::TryToRecoverKeySixteenthEncryptedToKeySixteenthHolder(uint32_t position,
                                                                                   uint32_t first_or_second_set,
                                                                                   Data data, RelayState &state,
                                                                                   Relay &relay)
{
    auto recipient_pubkey = state.GetKeySixteenthHoldersAsListOf16Recipients(relay.number, first_or_second_set)[position];
    CBigNum recipient_privkey = data.keydata[recipient_pubkey]["privkey"];
    CBigNum shared_secret = Hash(recipient_privkey * relay.public_key_sixteenths[position]);
    CBigNum encrypted_key_sixteenth = first_or_second_set == 1
                                      ? key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[position]
                                      : key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[position];
    CBigNum private_key_sixteenth = shared_secret ^ encrypted_key_sixteenth;
    if (Point(SECP256K1, private_key_sixteenth) != relay.public_key_sixteenths[position])
        return false;
    data.keydata[relay.public_key_sixteenths[position]]["privkey"] = private_key_sixteenth;
    return true;
}

bool KeyDistributionMessage::AuditKeySixteenthEncryptedInRelayJoinMessage(RelayJoinMessage &join_message,
                                                                          uint64_t position,
                                                                          MemoryDataStore &keydata)
{
    Point public_key = join_message.PublicKey();
    Point public_key_sixteenth = join_message.public_key_sixteenths[position];
    CBigNum private_key_sixteenth = keydata[public_key_sixteenth]["privkey"];
    CBigNum shared_secret = Hash(private_key_sixteenth * public_key);
    return private_key_sixteenth == (shared_secret ^ join_message.encrypted_private_key_sixteenths[position]);
}
