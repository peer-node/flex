#include <src/vector_tools.h>
#include "GoodbyeMessage.h"
#include "Relay.h"
#include "RelayState.h"


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
        for (uint8_t position = 0; position < sharer.key_quarter_holders.size(); position++)
            if (sharer.key_quarter_holders[position] == dead_relay_number)
                PopulateEncryptedKeySixteenth(&sharer, successor, position, data);
}

void GoodbyeMessage::PopulateEncryptedKeySixteenth(Relay *sharer, Relay *successor, uint8_t position, Data data)
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