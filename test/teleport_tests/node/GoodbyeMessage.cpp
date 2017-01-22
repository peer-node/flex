#include <src/vector_tools.h>
#include "GoodbyeMessage.h"
#include "Relay.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "GoodbyeMessage.cpp"

Point GoodbyeMessage::VerificationKey(Data data)
{
    Relay *relay = data.relay_state->GetRelayByNumber(dead_relay_number);
    if (relay == NULL)
        return Point(SECP256K1, 0);
    return relay->public_signing_key;
}

void GoodbyeMessage::PopulateEncryptedKeySixteenths(Data data)
{
    Relay *leaving_relay = data.relay_state->GetRelayByNumber(dead_relay_number);
    Relay *successor = data.relay_state->GetRelayByNumber(successor_relay_number);

    for (Relay &sharer : data.relay_state->relays)
        for (uint8_t position = 0; position < sharer.holders.key_quarter_holders.size(); position++)
            if (sharer.holders.key_quarter_holders[position] == dead_relay_number)
                PopulateFourEncryptedKeySixteenths(&sharer, successor, position, data);
}

void GoodbyeMessage::PopulateFourEncryptedKeySixteenths(Relay *sharer, Relay *successor, uint8_t position, Data data)
{
    key_quarter_sharers.push_back(sharer->number);
    key_quarter_positions.push_back(position);
    encrypted_key_sixteenths.push_back(std::vector<CBigNum>());
    for (uint32_t key_part_position = (uint32_t)position * 4; key_part_position < (position + 1) * 4; key_part_position++)
    {
        CBigNum private_key_sixteenth = data.keydata[sharer->PublicKeySixteenths()[key_part_position]]["privkey"];
        encrypted_key_sixteenths.back().push_back(successor->EncryptSecret(private_key_sixteenth));
    }
}

Relay *GoodbyeMessage::GetSuccessor(Data data)
{
    return data.relay_state->GetRelayByNumber(successor_relay_number);
}

bool GoodbyeMessage::ExtractSecrets(Data data)
{
    Relay *successor = data.relay_state->GetRelayByNumber(successor_relay_number);
    uint32_t key_sharer_position = 0;

    for (Relay &sharer : data.relay_state->relays)
        for (uint8_t quarter_holder_position = 0; quarter_holder_position < sharer.holders.key_quarter_holders.size();
                                                  quarter_holder_position++)
            if (sharer.holders.key_quarter_holders[quarter_holder_position] == dead_relay_number)
            {
                if (not DecryptFourKeySixteenths(&sharer, successor, quarter_holder_position, key_sharer_position, data))
                    return false;
                key_sharer_position++;
            }
    return true;
}

bool GoodbyeMessage::DecryptFourKeySixteenths(Relay *sharer, Relay *successor, uint8_t quarter_holder_position,
                                              uint32_t key_sharer_position, Data data)
{
    auto public_key_sixteenths = sharer->PublicKeySixteenths();

    for (uint32_t key_sixteenth_position = (uint32_t)quarter_holder_position * 4;
         key_sixteenth_position < (quarter_holder_position + 1) * 4; key_sixteenth_position++)
    {
        auto enclosed_key_part_position = key_sixteenth_position - 4 * quarter_holder_position;
        auto public_key_sixteenth = public_key_sixteenths[key_sixteenth_position];
        auto encrypted_private_key_sixteenth = encrypted_key_sixteenths[key_sharer_position][enclosed_key_part_position];

        auto private_key_sixteenth = successor->DecryptSecret(encrypted_private_key_sixteenth,
                                                              public_key_sixteenth, data);

        if (private_key_sixteenth == 0)
            return false;

        data.keydata[public_key_sixteenth]["privkey"] = private_key_sixteenth;
    }
    return true;
}