#include <src/vector_tools.h>
#include "GoodbyeMessage.h"
#include "Relay.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "GoodbyeMessage.cpp"

using std::vector;

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
    Relay *successor = data.relay_state->GetRelayByNumber(successor_number);

    for (Relay &sharer : data.relay_state->relays)
        for (uint8_t position = 0; position < sharer.holders.key_quarter_holders.size(); position++)
            if (sharer.holders.key_quarter_holders[position] == dead_relay_number)
                PopulateFourEncryptedKeySixteenths(&sharer, successor, position, data);
}

void GoodbyeMessage::PopulateFourEncryptedKeySixteenths(Relay *sharer, Relay *successor, uint8_t position, Data data)
{
    key_quarter_sharers.push_back(sharer->number);
    key_quarter_positions.push_back(position);
    encrypted_key_sixteenths.push_back(std::vector<uint256>());
    for (uint32_t key_part_position = (uint32_t)position * 4; key_part_position < (position + 1) * 4; key_part_position++)
    {
        CBigNum private_key_sixteenth = data.keydata[sharer->PublicKeySixteenths()[key_part_position]]["privkey"];
        encrypted_key_sixteenths.back().push_back(successor->EncryptSecret(private_key_sixteenth));
    }
}

Relay *GoodbyeMessage::GetSuccessor(Data data)
{
    return data.relay_state->GetRelayByNumber(successor_number);
}

bool GoodbyeMessage::ExtractSecrets(Data data)
{
    uint32_t key_sharer_position, encrypted_key_sixteenth_position;
    return ExtractSecrets(data, key_sharer_position, encrypted_key_sixteenth_position);
}

bool GoodbyeMessage::ExtractSecrets(Data data, uint32_t& key_sharer_position,
                                               uint32_t &encrypted_key_sixteenth_position)
{
    Relay *successor = data.relay_state->GetRelayByNumber(successor_number);
    key_sharer_position = 0;

    for (Relay &sharer : data.relay_state->relays)
        for (uint8_t quarter_holder_position = 0; quarter_holder_position < sharer.holders.key_quarter_holders.size();
                                                  quarter_holder_position++)
            if (sharer.holders.key_quarter_holders[quarter_holder_position] == dead_relay_number)
            {
                if (not DecryptFourKeySixteenths(&sharer, successor, quarter_holder_position, key_sharer_position,
                                                 encrypted_key_sixteenth_position, data))
                    return false;
                key_sharer_position++;
            }
    return true;
}

bool GoodbyeMessage::DecryptFourKeySixteenths(Relay *sharer, Relay *successor, uint8_t quarter_holder_position,
                                              uint32_t key_sharer_position,
                                              uint32_t &encrypted_key_sixteenth_position, Data data)
{
    auto public_key_sixteenths = sharer->PublicKeySixteenths();

    for (encrypted_key_sixteenth_position = 0; encrypted_key_sixteenth_position < 4; encrypted_key_sixteenth_position++)
    {
        uint32_t key_sixteenth_position = 4 * quarter_holder_position + encrypted_key_sixteenth_position;
        auto public_key_sixteenth = public_key_sixteenths[key_sixteenth_position];
        auto encrypted_key_sixteenth = encrypted_key_sixteenths[key_sharer_position][encrypted_key_sixteenth_position];

        auto private_key_sixteenth = successor->DecryptSecret(encrypted_key_sixteenth, public_key_sixteenth, data);

        if (private_key_sixteenth == 0)
            return false;

        data.keydata[public_key_sixteenth]["privkey"] = private_key_sixteenth;
    }
    return true;
}

bool GoodbyeMessage::ValidateSizes()
{
    uint64_t number_of_key_sharers = key_quarter_sharers.size();
    if (key_quarter_positions.size() != number_of_key_sharers or
            encrypted_key_sixteenths.size() != number_of_key_sharers)
        return false;
    for (auto &set_of_sixteenths : encrypted_key_sixteenths)
        if (set_of_sixteenths.size() != 4)
            return false;
    return true;
}

bool GoodbyeMessage::IsValid(Data data)
{
    if (not (ValidateSizes()))
        return false;

    auto dead_relay = data.relay_state->GetRelayByNumber(dead_relay_number);
    auto generated_successor_number = data.relay_state->AssignSuccessorToRelay(dead_relay);

    if (successor_number != generated_successor_number)
        return false;

    if (not CheckKeyQuarterSharersAndPositions(data))
        return false;;

    return true;
}

bool GoodbyeMessage::CheckKeyQuarterSharersAndPositions(Data data)
{
    vector<uint64_t> key_quarter_sharers_;
    vector<uint8_t> key_quarter_positions_;
    for (Relay &sharer : data.relay_state->relays)
        for (uint8_t position = 0; position < sharer.holders.key_quarter_holders.size(); position++)
            if (sharer.holders.key_quarter_holders[position] == dead_relay_number)
            {
                key_quarter_sharers_.push_back(sharer.number);
                key_quarter_positions_.push_back(position);
            }
    return key_quarter_sharers == key_quarter_sharers_ and key_quarter_positions == key_quarter_positions_;
}