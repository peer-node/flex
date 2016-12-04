#include <src/vector_tools.h>
#include <src/credits/creditsign.h>
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "RelayState.cpp"

void RelayState::ProcessRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    Relay relay = GenerateRelayFromRelayJoinMessage(relay_join_message);
    relays.push_back(relay);
}

Relay RelayState::GenerateRelayFromRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    Relay new_relay;
    if (relays.size() == 0)
        new_relay.number = 1;
    else
        new_relay.number = relays.back().number + 1;

    new_relay.join_message_hash = relay_join_message.GetHash160();
    new_relay.public_key = relay_join_message.PublicKey();
    new_relay.public_key_sixteenths = relay_join_message.public_key_sixteenths;
    return new_relay;
}

void RelayState::AssignRemainingQuarterKeyHoldersToRelay(Relay &relay, uint160 encoding_message_hash)
{
    while (relay.quarter_key_holders.size() < 4)
        relay.quarter_key_holders.push_back(SelectKeyHolderWhichIsntAlreadyBeingUsed(relay, encoding_message_hash));
}

uint64_t RelayState::KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed)
{
    uint64_t chosen_relay_number = SelectKeyHolderWhichIsntAlreadyBeingUsed(relay, random_seed);
    while (chosen_relay_number <= relay.number)
    {
        random_seed = HashUint160(random_seed);
        chosen_relay_number = SelectKeyHolderWhichIsntAlreadyBeingUsed(relay, random_seed);
    }
    return chosen_relay_number;
}

uint64_t RelayState::SelectKeyHolderWhichIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed)
{
    bool found = false;
    uint64_t relay_position = 0;
    while (not found)
    {
        random_seed = HashUint160(random_seed);
        relay_position = (CBigNum(random_seed) % CBigNum(relays.size())).getulong();
        if (relays[relay_position].number != relay.number and not KeyPartHolderIsAlreadyBeingUsed(relay, relays[relay_position]))
        {
            found = true;
        }
    }
    return relays[relay_position].number;
}

bool RelayState::KeyPartHolderIsAlreadyBeingUsed(Relay &key_distributor, Relay &key_part_holder)
{
    for (auto key_part_holder_group : key_distributor.KeyPartHolderGroups())
    {
        for (auto number_of_existing_key_part_holder : key_part_holder_group)
            if (number_of_existing_key_part_holder == key_part_holder.number)
            {
                return true;
            }
    }
    return false;
}

void RelayState::AssignRemainingKeySixteenthHoldersToRelay(Relay &relay, uint160 encoding_message_hash)
{
    while (relay.first_set_of_key_sixteenth_holders.size() < 16)
        relay.first_set_of_key_sixteenth_holders.push_back(SelectKeyHolderWhichIsntAlreadyBeingUsed(relay, encoding_message_hash));
    while (relay.second_set_of_key_sixteenth_holders.size() < 16)
        relay.second_set_of_key_sixteenth_holders.push_back(SelectKeyHolderWhichIsntAlreadyBeingUsed(relay, encoding_message_hash));
}

bool RelayState::IsDead(uint64_t relay_number)
{
    return false;
}

Relay &RelayState::GetRelayByNumber(uint64_t number)
{
    for (auto & relay : relays)
        if (relay.number == number)
            return relay;
    return relays[0];
}

bool RelayState::ThereAreEnoughRelaysToAssignKeyPartHolders(Relay &relay)
{
    uint64_t number_of_relays_that_joined_later = NumberOfRelaysThatJoinedLaterThan(relay);
    uint64_t number_of_relays_that_joined_before = NumberOfRelaysThatJoinedBefore(relay);

    return number_of_relays_that_joined_later >= 3 and
            number_of_relays_that_joined_before + number_of_relays_that_joined_later >= 36;
}

void RelayState::RemoveKeyPartHolders(Relay &relay)
{
    relay.quarter_key_holders.resize(0);
    relay.first_set_of_key_sixteenth_holders.resize(0);
    relay.second_set_of_key_sixteenth_holders.resize(0);
}

bool RelayState::AssignKeyPartHoldersToRelay(Relay &relay, uint160 encoding_message_hash)
{
    if (not ThereAreEnoughRelaysToAssignKeyPartHolders(relay))
        return false;
    RemoveKeyPartHolders(relay);
    AssignKeySixteenthAndQuarterHoldersWhoJoinedLater(relay, encoding_message_hash);
    AssignRemainingQuarterKeyHoldersToRelay(relay, encoding_message_hash);
    AssignRemainingKeySixteenthHoldersToRelay(relay, encoding_message_hash);
    return true;
}

void RelayState::AssignKeySixteenthAndQuarterHoldersWhoJoinedLater(Relay &relay, uint160 encoding_message_hash)
{
    uint64_t key_part_holder = KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(relay, encoding_message_hash);
    relay.quarter_key_holders.push_back(key_part_holder);
    key_part_holder = KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(relay, encoding_message_hash);
    relay.first_set_of_key_sixteenth_holders.push_back(key_part_holder);
    key_part_holder = KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(relay, encoding_message_hash);
    relay.second_set_of_key_sixteenth_holders.push_back(key_part_holder);
}

uint64_t RelayState::NumberOfRelaysThatJoinedLaterThan(Relay &relay)
{
    uint64_t count = 0;
    for (uint64_t n = relays.size(); n > 0 and relays[n - 1].number > relay.number; n--)
        count++;
    return count;
}

uint64_t RelayState::NumberOfRelaysThatJoinedBefore(Relay &relay)
{
    uint64_t count = 0;
    for (uint64_t n = 0; n < relays.size() and relays[n].number < relay.number; n++)
        count++;
    return count;
}

std::vector<Point> RelayState::GetKeyQuarterHoldersAsListOf16Recipients(uint64_t relay_number)
{
    Relay &relay = GetRelayByNumber(relay_number);
    std::vector<Point> recipient_public_keys;
    for (auto quarter_key_holder : relay.quarter_key_holders)
    {
        Relay &recipient = GetRelayByNumber(quarter_key_holder);
        for (uint64_t i = 0; i < 4; i++)
            recipient_public_keys.push_back(recipient.public_key);
    }
    return recipient_public_keys;
}

std::vector<Point>
RelayState::GetKeySixteenthHoldersAsListOf16Recipients(uint64_t relay_number, uint64_t first_or_second_set)
{
    Relay &relay = GetRelayByNumber(relay_number);
    std::vector<Point> recipient_public_keys;
    std::vector<uint64_t> &recipients = (first_or_second_set == 1) ? relay.first_set_of_key_sixteenth_holders
                                                                   : relay.second_set_of_key_sixteenth_holders;

    for (auto key_sixteenth_holder : recipients)
        recipient_public_keys.push_back(GetRelayByNumber(key_sixteenth_holder).public_key);

    return recipient_public_keys;
}
