#include "SecretRecoveryMessage.h"
#include "RelayState.h"

Point SecretRecoveryMessage::VerificationKey(Data data)
{
    Relay *relay = data.relay_state->GetRelayByJoinHash(quarter_holder_number);
    if (relay == NULL)
        return Point(SECP256K1, 0);
    return relay->public_key;
}