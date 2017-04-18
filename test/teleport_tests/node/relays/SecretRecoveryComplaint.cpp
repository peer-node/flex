#include "SecretRecoveryComplaint.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "SecretRecoveryComplaint.cpp"


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

Relay *SecretRecoveryComplaint::GetDeadRelay(Data data)
{
    auto secret_recovery_message = GetSecretRecoveryMessage(data);
    return data.relay_state->GetRelayByNumber(secret_recovery_message.dead_relay_number);
}

SecretRecoveryMessage SecretRecoveryComplaint::GetSecretRecoveryMessage(Data data)
{
    return data.msgdata[secret_recovery_message_hash]["secret_recovery"];
}

void SecretRecoveryComplaint::Populate(SecretRecoveryMessage recovery_message, uint32_t key_sharer_position,
                                       uint32_t key_part_position, Data data)
{
    successor_number = recovery_message.successor_number;
    secret_recovery_message_hash = recovery_message.GetHash160();
    position_of_key_sharer = key_sharer_position;
    position_of_bad_encrypted_secret = key_part_position;

    auto complainer = GetComplainer(data);
    if (complainer == NULL)
        return;

    auto &points = recovery_message.sextets_of_points_corresponding_to_shared_secret_quarters;
    auto corresponding_point = points[key_sharer_position][key_part_position];
    private_receiving_key = complainer->GenerateRecipientPrivateKey(corresponding_point, data).getuint256();
}

bool SecretRecoveryComplaint::IsValid(Data data)
{
    auto recovery_message = GetSecretRecoveryMessage(data);
    if (position_of_key_sharer >= recovery_message.key_quarter_sharers.size())
        return false;
    if (position_of_bad_encrypted_secret >= 4)
        return false;
    if (SuccessionCompletedMessageHasBeenReceived(data))
        return false;
    if (not PrivateReceivingKeyIsCorrect(data))
        return false;
    if (EncryptedSecretIsOk(data))
        return false;;
    return true;
}

bool SecretRecoveryComplaint::SuccessionCompletedMessageHasBeenReceived(Data data)
{
    auto dead_relay = GetDeadRelay(data);

    if (dead_relay == NULL)
        return false;

    auto &attempt = dead_relay->succession_attempts[successor_number];
    return attempt.succession_completed_message_hash != 0;
}

bool SecretRecoveryComplaint::PrivateReceivingKeyIsCorrect(Data data)
{
    Point point_corresponding_to_secret = GetPointCorrespondingToSecret(data);

    auto complainer = GetComplainer(data);
    if (complainer == NULL)
        return false;

    Point public_receiving_key = complainer->GenerateRecipientPublicKey(point_corresponding_to_secret);
    return Point(CBigNum(private_receiving_key)) == public_receiving_key;
}

bool SecretRecoveryComplaint::EncryptedSecretIsOk(Data data)
{
    auto recovered_secret = RecoverSecret(data);
    Point point_corresponding_to_secret = GetPointCorrespondingToSecret(data);
    Point shared_secret_quarter = DecryptPointUsingHexPrefixes(recovered_secret, point_corresponding_to_secret);
    return GetPointCorrespondingToSecret(data) == Point(StorePointInBigNum(shared_secret_quarter));
}

CBigNum SecretRecoveryComplaint::RecoverSecret(Data data)
{
    CBigNum shared_secret = Hash(private_receiving_key * GetPointCorrespondingToSecret(data));
    return shared_secret ^ CBigNum(GetEncryptedSecret(data));
}

uint256 SecretRecoveryComplaint::GetEncryptedSecret(Data data)
{
    auto recovery_message = GetSecretRecoveryMessage(data);
    auto &quartets = recovery_message.sextets_of_encrypted_shared_secret_quarters;
    return quartets[position_of_key_sharer][position_of_bad_encrypted_secret];
}

Point SecretRecoveryComplaint::GetPointCorrespondingToSecret(Data data)
{
    auto recovery_message = GetSecretRecoveryMessage(data);
    auto &points = recovery_message.sextets_of_points_corresponding_to_shared_secret_quarters;
    return points[position_of_key_sharer][position_of_bad_encrypted_secret];
}