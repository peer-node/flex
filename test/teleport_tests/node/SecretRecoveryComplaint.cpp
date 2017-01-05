#include "SecretRecoveryComplaint.h"
#include "RelayState.h"

Point SecretRecoveryComplaint::VerificationKey(Data data)
{
    Relay *dead_relay = GetComplainer(data);
    if (dead_relay == NULL)
        return Point(SECP256K1, 0);
    return dead_relay->public_signing_key;
}

Relay *SecretRecoveryComplaint::GetComplainer(Data data)
{
    auto secret_recovery_message = GetSecretRecoveryMessage(data);
    return data.relay_state->GetRelayByNumber(secret_recovery_message.dead_relay_number);
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