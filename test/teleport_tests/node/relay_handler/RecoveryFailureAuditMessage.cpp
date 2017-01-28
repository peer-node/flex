#include "RecoveryFailureAuditMessage.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "RecoveryFailureAuditMessage.cpp"

Point RecoveryFailureAuditMessage::VerificationKey(Data data)
{
    auto quarter_holder = data.relay_state->GetRelayByNumber(quarter_holder_number);

    if (quarter_holder == NULL)
        return Point(CBigNum(0));

    return quarter_holder->public_signing_key;
}

void RecoveryFailureAuditMessage::Populate(SecretRecoveryFailureMessage failure_message,
                                           SecretRecoveryMessage recovery_message,
                                           Data data)
{
    failure_message_hash = failure_message.GetHash160();
    quarter_holder_number = recovery_message.quarter_holder_number;
    private_receiving_key_quarter = GetReceivingKeyQuarter(failure_message, recovery_message, data).getuint256();
}

CBigNum RecoveryFailureAuditMessage::GetReceivingKeyQuarter(SecretRecoveryFailureMessage failure_message,
                                                            SecretRecoveryMessage recovery_message, Data data)
{
    auto dead_relay = data.relay_state->GetRelayByNumber(recovery_message.dead_relay_number);
    Point public_key_sixteenth = GetPublicKeySixteenth(failure_message, recovery_message, data);

    auto quarter_holder_position = PositionOfEntryInVector(quarter_holder_number, dead_relay->holders.key_quarter_holders);
    return dead_relay->GenerateRecipientPrivateKeyQuarter(public_key_sixteenth, (uint8_t)quarter_holder_position, data);
}

uint8_t RecoveryFailureAuditMessage::GetKeyQuarterPosition(SecretRecoveryFailureMessage failure_message,
                                                           SecretRecoveryMessage recovery_message, Data data)
{
    auto dead_relay = data.relay_state->GetRelayByNumber(recovery_message.dead_relay_number);
    auto key_sharer = GetKeySharer(failure_message, recovery_message, data);

    return (uint8_t) PositionOfEntryInVector(dead_relay->number, key_sharer->holders.key_quarter_holders);
}

Point RecoveryFailureAuditMessage::GetPublicKeySixteenth(SecretRecoveryFailureMessage failure_message,
                                                         SecretRecoveryMessage recovery_message, Data data)
{
    auto key_sharer = GetKeySharer(failure_message, recovery_message, data);

    auto key_sixteenth_position = GetKeySixteenthPosition(failure_message, recovery_message, data);
    return key_sharer->PublicKeySixteenths()[key_sixteenth_position];
}

Point RecoveryFailureAuditMessage::GetPublicKeySixteenth(Data data)
{
    auto recovery_message = GetSecretRecoveryMessage(data);
    auto failure_message = GetSecretRecoveryFailureMessage(data);

    return GetPublicKeySixteenth(failure_message, recovery_message, data);
}

int64_t RecoveryFailureAuditMessage::GetKeySixteenthPosition(SecretRecoveryFailureMessage failure_message,
                                                             SecretRecoveryMessage recovery_message, Data data)
{
    return 4 * GetKeyQuarterPosition(failure_message, recovery_message, data)
                + failure_message.shared_secret_quarter_position;
}

int64_t RecoveryFailureAuditMessage::GetKeySixteenthPosition(Data data)
{
    auto recovery_message = GetSecretRecoveryMessage(data);
    auto failure_message = GetSecretRecoveryFailureMessage(data);

    return GetKeySixteenthPosition(failure_message, recovery_message, data);
}

Relay *RecoveryFailureAuditMessage::GetKeySharer(SecretRecoveryFailureMessage failure_message,
                                                 SecretRecoveryMessage recovery_message, Data data)
{
    auto key_sharer_number = recovery_message.key_quarter_sharers[failure_message.key_sharer_position];
    return data.relay_state->GetRelayByNumber(key_sharer_number);
}

Relay *RecoveryFailureAuditMessage::GetKeySharer(Data data)
{
    auto recovery_message = GetSecretRecoveryMessage(data);
    auto failure_message = GetSecretRecoveryFailureMessage(data);

    return GetKeySharer(failure_message, recovery_message, data);
}

bool RecoveryFailureAuditMessage::VerifyPrivateReceivingKeyQuarterMatchesPublicReceivingKeyQuarter(Data data)
{
    SecretRecoveryFailureMessage failure_message = data.GetMessage(failure_message_hash);
    auto recovery_message = GetSecretRecoveryMessage(data);
    Point public_key_sixteenth = GetPublicKeySixteenth(failure_message, recovery_message, data);
    auto dead_relay = data.relay_state->GetRelayByNumber(recovery_message.dead_relay_number);

    auto quarter_holder_position = PositionOfEntryInVector(quarter_holder_number,
                                                           dead_relay->holders.key_quarter_holders);
    auto public_receiving_key_quarter = dead_relay->GenerateRecipientPublicKeyQuarter(public_key_sixteenth,
                                                                                      (uint8_t) quarter_holder_position);
    return Point(CBigNum(private_receiving_key_quarter)) == public_receiving_key_quarter;
}

SecretRecoveryMessage RecoveryFailureAuditMessage::GetSecretRecoveryMessage(Data data)
{
    SecretRecoveryFailureMessage failure_message = GetSecretRecoveryFailureMessage(data);

    for (auto recovery_message_hash : failure_message.recovery_message_hashes)
    {
        SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hash);
        if (recovery_message.quarter_holder_number == quarter_holder_number)
            return recovery_message;
    }
    return SecretRecoveryMessage();
}

SecretRecoveryFailureMessage RecoveryFailureAuditMessage::GetSecretRecoveryFailureMessage(Data data)
{
    return data.GetMessage(failure_message_hash);
}

bool RecoveryFailureAuditMessage::VerifyEncryptedSharedSecretQuarterInSecretRecoveryMessageWasCorrect(Data data)
{
    auto recovery_message = GetSecretRecoveryMessage(data);

    auto encrypted_shared_secret_quarter = GetEncryptedSharedSecretQuarterFromSecretRecoveryMessage(data);

    auto recipient = data.relay_state->GetRelayByNumber(recovery_message.successor_number);

    auto shared_secret_quarter = GetSharedSecretQuarter(data);
    auto newly_encrypted_shared_secret_quarter = recipient->EncryptSecretPoint(shared_secret_quarter);
    return newly_encrypted_shared_secret_quarter == encrypted_shared_secret_quarter;
}

uint256 RecoveryFailureAuditMessage::GetEncryptedSharedSecretQuarterFromSecretRecoveryMessage(Data data)
{
    auto recovery_message = GetSecretRecoveryMessage(data);
    auto failure_message = GetSecretRecoveryFailureMessage(data);

    auto &encrypted_secrets = recovery_message.quartets_of_encrypted_shared_secret_quarters;

    return encrypted_secrets[failure_message.key_sharer_position][failure_message.shared_secret_quarter_position];
}

Relay *RecoveryFailureAuditMessage::GetDeadRelay(Data data)
{
    return data.relay_state->GetRelayByNumber(GetSecretRecoveryMessage(data).dead_relay_number);
}

Point RecoveryFailureAuditMessage::GetSharedSecretQuarter(Data data)
{
    Point public_key_sixteenth = GetPublicKeySixteenth(data);

    return private_receiving_key_quarter * public_key_sixteenth;
}

bool RecoveryFailureAuditMessage::IsValid(Data data)
{
    auto dead_relay = GetDeadRelay(data);

    if (dead_relay == NULL or not VectorContainsEntry(dead_relay->holders.key_quarter_holders, quarter_holder_number))
        return false;;

    return true;
}
