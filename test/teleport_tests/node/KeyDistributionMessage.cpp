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
    std::vector<uint64_t> recipient_relay_numbers = relay_state.GetKeyQuarterHoldersAsListOf16RelayNumbers(relay.number);

    for (uint64_t i = 0; i < private_key_sixteenths.size(); i++)
    {
        Relay *recipient = relay_state.GetRelayByNumber(recipient_relay_numbers[i]);
        if (recipient == NULL)
            throw RelayStateException("referenced relay does not exist");

        key_sixteenths_encrypted_for_key_quarter_holders.push_back(recipient->EncryptSecret(private_key_sixteenths[i]));
    }
}

void KeyDistributionMessage::PopulateKeySixteenthsEncryptedForKeySixteenthHolders(MemoryDataStore &keydata,
                                                                                  Relay &relay,
                                                                                  RelayState &relay_state)
{
    auto private_key_sixteenths = relay.PrivateKeySixteenths(keydata);
    auto first_set_of_recipients = relay_state.GetKeySixteenthHoldersAsListOf16RelayNumbers(relay.number, 1);
    auto second_set_of_recipients = relay_state.GetKeySixteenthHoldersAsListOf16RelayNumbers(relay.number, 2);

    for (uint64_t i = 0; i < private_key_sixteenths.size(); i++)
    {
        Relay *recipient1 = relay_state.GetRelayByNumber(first_set_of_recipients[i]);
        Relay *recipient2 = relay_state.GetRelayByNumber(second_set_of_recipients[i]);
        if (recipient1 == NULL or recipient2 == NULL)
            throw RelayStateException("referenced relay does not exist");
        key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders.push_back(
                recipient1->EncryptSecret(private_key_sixteenths[i]));
        key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders.push_back(
                recipient2->EncryptSecret(private_key_sixteenths[i]));
    }
}

bool KeyDistributionMessage::KeyQuarterHolderPrivateKeyIsAvailable(uint64_t position, Data data,
                                                                   RelayState &relay_state, Relay &relay)
{
    auto recipients = relay_state.GetKeyQuarterHoldersAsListOf16RelayNumbers(relay.number);
    Relay *recipient = relay_state.GetRelayByNumber(recipients[position]);
    if (recipient == NULL)
        throw RelayStateException("referenced relay does not exist");
    return data.keydata[recipient->public_signing_key].HasProperty("privkey");
}

bool KeyDistributionMessage::KeySixteenthForKeyQuarterHolderIsCorrectlyEncrypted(uint64_t position, Data data,
                                                                                 RelayState &relay_state, Relay &relay)
{
    auto recipients = relay_state.GetKeyQuarterHoldersAsListOf16RelayNumbers(relay.number);
    Relay *recipient = relay_state.GetRelayByNumber(recipients[position]);
    Point public_key_sixteenth = relay.PublicKeySixteenths()[position];
    CBigNum private_key_sixteenth = data.keydata[public_key_sixteenth]["privkey"];
    CBigNum encrypted_private_key_sixteenth = recipient->EncryptSecret(private_key_sixteenth);
    return key_sixteenths_encrypted_for_key_quarter_holders[position] == encrypted_private_key_sixteenth;
}

bool KeyDistributionMessage::KeySixteenthHolderPrivateKeyIsAvailable(uint32_t position, uint32_t first_or_second_set,
                                                                     Data data, RelayState &relay_state,
                                                                     Relay &relay)
{
    auto recipients = relay_state.GetKeySixteenthHoldersAsListOf16RelayNumbers(relay.number, first_or_second_set);
    Relay *recipient = relay_state.GetRelayByNumber(recipients[position]);
    if (recipient == NULL)
        throw RelayStateException("no such relay");
    return data.keydata[recipient->public_signing_key].HasProperty("privkey");
}

bool KeyDistributionMessage::KeySixteenthForKeySixteenthHolderIsCorrectlyEncrypted(uint64_t position,
                                                                                   uint32_t first_or_second_set,
                                                                                   Data data, RelayState &state,
                                                                                   Relay &relay)
{
    auto recipients = state.GetKeySixteenthHoldersAsListOf16RelayNumbers(relay.number, first_or_second_set);
    Relay *recipient = state.GetRelayByNumber(recipients[position]);
    Point public_key_sixteenth = relay.PublicKeySixteenths()[position];
    CBigNum encrypted_key_sixteenth = first_or_second_set == 1
                                      ? key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[position]
                                      : key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[position];

    CBigNum private_key_sixteenth = recipient->DecryptSecret(encrypted_key_sixteenth, public_key_sixteenth, data);
    if (private_key_sixteenth == 0)
        return false;
    data.keydata[relay.PublicKeySixteenths()[position]]["privkey"] = private_key_sixteenth;
    return true;
}

bool KeyDistributionMessage::TryToRecoverKeySixteenthEncryptedToQuarterKeyHolder(uint32_t position, Data data,
                                                                                 RelayState &state, Relay &relay)
{
    auto recipients = state.GetKeyQuarterHoldersAsListOf16RelayNumbers(relay.number);
    Relay *recipient = state.GetRelayByNumber(recipients[position]);
    CBigNum private_key_sixteenth = recipient->DecryptSecret(key_sixteenths_encrypted_for_key_quarter_holders[position],
                                                             relay.PublicKeySixteenths()[position], data);
    if (private_key_sixteenth == 0)
        return false;
    data.keydata[relay.PublicKeySixteenths()[position]]["privkey"] = private_key_sixteenth;
    return true;
}

bool KeyDistributionMessage::TryToRecoverKeySixteenthEncryptedToKeySixteenthHolder(uint32_t position,
                                                                                   uint32_t first_or_second_set,
                                                                                   Data data, RelayState &state,
                                                                                   Relay &relay)
{
    auto recipients = state.GetKeySixteenthHoldersAsListOf16RelayNumbers(relay.number, first_or_second_set);
    Relay *recipient = state.GetRelayByNumber(recipients[position]);
    CBigNum encrypted_key_sixteenth = first_or_second_set == 1
                                      ? key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[position]
                                      : key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[position];

    CBigNum private_key_sixteenth = recipient->DecryptSecret(encrypted_key_sixteenth,
                                                             relay.PublicKeySixteenths()[position], data);

    if (private_key_sixteenth == 0)
        return false;
    data.keydata[relay.PublicKeySixteenths()[position]]["privkey"] = private_key_sixteenth;
    return true;
}

bool KeyDistributionMessage::AuditKeySixteenthEncryptedInRelayJoinMessage(RelayJoinMessage &join_message,
                                                                          uint64_t position,
                                                                          MemoryDataStore &keydata)
{
    Point public_key = join_message.PublicSigningKey();
    Point public_key_sixteenth = join_message.public_key_set.PublicKeySixteenths()[position];
    CBigNum private_key_sixteenth = keydata[public_key_sixteenth]["privkey"];
    CBigNum shared_secret = Hash(private_key_sixteenth * public_key);
    return private_key_sixteenth == (shared_secret ^ join_message.encrypted_private_key_sixteenths[position]);
}
