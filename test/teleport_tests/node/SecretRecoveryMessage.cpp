#include "SecretRecoveryMessage.h"
#include "RelayState.h"

Point SecretRecoveryMessage::VerificationKey(Data data)
{
    Relay *relay = GetKeyQuarterHolder(data);
    if (relay == NULL)
        return Point(SECP256K1, 0);
    return relay->public_key;
}

Relay *SecretRecoveryMessage::GetKeyQuarterHolder(Data data)
{
    return data.relay_state->GetRelayByNumber(quarter_holder_number);
}

Relay *SecretRecoveryMessage::GetDeadRelay(Data data)
{
    return data.relay_state->GetRelayByNumber(dead_relay_number);
}

Relay *SecretRecoveryMessage::GetSuccessor(Data data)
{
    return data.relay_state->GetRelayByNumber(successor_number);
}
