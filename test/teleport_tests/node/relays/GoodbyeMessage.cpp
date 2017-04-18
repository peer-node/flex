#include <src/vector_tools.h>
#include "GoodbyeMessage.h"
#include "Relay.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "GoodbyeMessage.cpp"

using std::vector;

Point GoodbyeMessage::VerificationKey(Data data)
{
    Relay *relay = data.relay_state->GetRelayByNumber(dead_relay_number);
    if (relay == NULL)
        return Point(SECP256K1, 0);
    return relay->public_signing_key;
}
