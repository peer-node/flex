#include "GoodbyeComplaint.h"
#include "GoodbyeMessage.h"
#include "RelayState.h"

#include "log.h"

#define LOG_CATEGORY "GoodbyeComplaint.cpp"


Point GoodbyeComplaint::VerificationKey(Data data)
{
    Relay *complainer = GetComplainer(data);
    return complainer->public_signing_key;
}

Relay *GoodbyeComplaint::GetSecretSender(Data data)
{
    return data.relay_state->GetRelayByNumber(GetGoodbyeMessage(data).dead_relay_number);
}

GoodbyeMessage GoodbyeComplaint::GetGoodbyeMessage(Data data)
{
    return data.msgdata[goodbye_message_hash]["goodbye"];
}

Relay *GoodbyeComplaint::GetSuccessor(Data data)
{
    return data.relay_state->GetRelayByNumber(GetGoodbyeMessage(data).successor_relay_number);
}

Relay *GoodbyeComplaint::GetComplainer(Data data)
{
    return GetSuccessor(data);
}

Relay *GoodbyeComplaint::GetKeySharer(Data data)
{
    auto goodbye = GetGoodbyeMessage(data);
    uint64_t key_sharer_number = goodbye.key_quarter_sharers[key_sharer_position];
    return data.relay_state->GetRelayByNumber(key_sharer_number);
}

void GoodbyeComplaint::Populate(GoodbyeMessage &goodbye_message, Data data)
{
    goodbye_message_hash = goodbye_message.GetHash160();
    goodbye_message.ExtractSecrets(data, key_sharer_position, position_of_bad_encrypted_key_sixteenth);
    PopulateRecipientPrivateKey(data);
}

void GoodbyeComplaint::PopulateRecipientPrivateKey(Data data)
{
    auto recipient = GetSuccessor(data);
    auto key_sharer = GetKeySharer(data);
    auto quarter_holder_position = GetGoodbyeMessage(data).key_quarter_positions[key_sharer_position];
    auto key_sixteenth_position = position_of_bad_encrypted_key_sixteenth + 4 * quarter_holder_position;
    auto point_corresponding_to_secret = key_sharer->PublicKeySixteenths()[key_sixteenth_position];

    recipient_private_key = recipient->GenerateRecipientPrivateKey(point_corresponding_to_secret, data);
}

bool GoodbyeComplaint::IsValid(Data data)
{
    GoodbyeMessage goodbye_message = data.GetMessage(goodbye_message_hash);

    if (key_sharer_position >= goodbye_message.key_quarter_sharers.size())
        return false;

    if (position_of_bad_encrypted_key_sixteenth >= 4)
        return false;;

    return true;
}
