#include "SecretRecoveryComplaint.h"
#include "RelayState.h"

Point SecretRecoveryComplaint::VerificationKey(Data data)
{
    Relay *successor = GetComplainer(data);
    if (successor == NULL)
        return Point(CBigNum(0));
    return successor->public_signing_key;
}

Relay *SecretRecoveryComplaint::GetComplainer(Data data)
{
    auto secret_recovery_message = GetSecretRecoveryMessage(data);
    return data.relay_state->GetRelayByNumber(secret_recovery_message.successor_number);
}

Relay *SecretRecoveryComplaint::GetSecretSender(Data data)
{
    auto secret_recovery_message = GetSecretRecoveryMessage(data);
    return data.relay_state->GetRelayByNumber(secret_recovery_message.quarter_holder_number);
}

SecretRecoveryMessage SecretRecoveryComplaint::GetSecretRecoveryMessage(Data data)
{
    return data.msgdata[secret_recovery_message_hash]["secret_recovery"];
}

void SecretRecoveryComplaint::Populate(SecretRecoveryMessage recovery_message, uint32_t key_sharer_position,
                                       uint32_t key_part_position, Data data)
{
    secret_recovery_message_hash = recovery_message.GetHash160();
    position_of_key_sharer = key_sharer_position;
    position_of_bad_encrypted_secret = key_part_position;

    auto complainer = GetComplainer(data);
    if (complainer == NULL)
        return;

    auto &points = recovery_message.quartets_of_points_corresponding_to_shared_secret_quarters;
    auto corresponding_point = points[key_sharer_position][key_part_position];
    private_receiving_key = complainer->GenerateRecipientPrivateKey(corresponding_point, data);
}

bool SecretRecoveryComplaint::IsValid(Data data)
{
    auto recovery_message = GetSecretRecoveryMessage(data);
    if (position_of_key_sharer >= recovery_message.key_quarter_sharers.size())
        return false;
    if (position_of_bad_encrypted_secret >= 4)
        return false;;
    return true;
}
