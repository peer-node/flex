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
    return data.relay_state->GetRelayByNumber(GetGoodbyeMessage(data).successor_number);
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
    bool no_failure_found = key_sharer_position >= goodbye_message.key_quarter_sharers.size();
    if (no_failure_found)
        return;
    PopulateRecipientPrivateKey(data);
}

void GoodbyeComplaint::PopulateRecipientPrivateKey(Data data)
{
    auto recipient = GetSuccessor(data);
    auto point_corresponding_to_secret = GetPointCorrespondingToSecret(data);

    recipient_private_key = recipient->GenerateRecipientPrivateKey(point_corresponding_to_secret, data).getuint256();
}

Point GoodbyeComplaint::GetPointCorrespondingToSecret(Data data)
{
    auto key_sharer = GetKeySharer(data);
    auto quarter_holder_position = GetGoodbyeMessage(data).key_quarter_positions[key_sharer_position];
    auto key_sixteenth_position = position_of_bad_encrypted_key_sixteenth + 4 * quarter_holder_position;
    return key_sharer->PublicKeySixteenths()[key_sixteenth_position];
}

bool GoodbyeComplaint::IsValid(Data data)
{
    GoodbyeMessage goodbye_message = data.GetMessage(goodbye_message_hash);

    if (key_sharer_position >= goodbye_message.key_quarter_sharers.size())
        return false;
    if (position_of_bad_encrypted_key_sixteenth >= 4)
        return false;
    if (SuccessorHasAlreadySentASuccessionCompletedMessage(data))
        return false;
    if (PrivateReceivingKeyIsIncorrect(data))
        return false;
    if (EncryptedSecretIsOk(data))
        return false;;
    return true;
}

bool GoodbyeComplaint::SuccessorHasAlreadySentASuccessionCompletedMessage(Data data)
{
    GoodbyeMessage goodbye_message = data.GetMessage(goodbye_message_hash);
    auto successor = data.relay_state->GetRelayByNumber(goodbye_message.successor_number);
    for (auto succession_completed_message_hash : successor->hashes.succession_completed_message_hashes)
    {
        SuccessionCompletedMessage succession_completed_message = data.GetMessage(succession_completed_message_hash);
        if (succession_completed_message.goodbye_message_hash == goodbye_message_hash)
            return true;
    }
    return false;
}

bool GoodbyeComplaint::PrivateReceivingKeyIsIncorrect(Data data)
{
    auto point_corresponding_to_secret = GetPointCorrespondingToSecret(data);
    auto successor = GetSuccessor(data);
    auto public_receiving_key = successor->GenerateRecipientPublicKey(point_corresponding_to_secret);
    return Point(CBigNum(recipient_private_key)) != public_receiving_key;
}

bool GoodbyeComplaint::EncryptedSecretIsOk(Data data)
{
    auto point_corresponding_to_secret = GetPointCorrespondingToSecret(data);
    uint256 encrypted_secret = GetEncryptedSecret(data);
    CBigNum shared_secret = Hash(recipient_private_key * point_corresponding_to_secret);
    CBigNum decrypted_secret = CBigNum(encrypted_secret) ^ shared_secret;
    return Point(decrypted_secret) == point_corresponding_to_secret;
}

uint256 GoodbyeComplaint::GetEncryptedSecret(Data data)
{
    auto goodbye_message = GetGoodbyeMessage(data);
    return goodbye_message.encrypted_key_sixteenths[key_sharer_position][position_of_bad_encrypted_key_sixteenth];
}