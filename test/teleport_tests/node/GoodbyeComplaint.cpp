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
    GoodbyeMessage goodbye_message = data.msgdata[goodbye_message_hash]["goodbye"];
    return data.relay_state->GetRelayByNumber(goodbye_message.dead_relay_number);
}

Relay *GoodbyeComplaint::GetComplainer(Data data)
{
    Relay *dead_relay = GetSecretSender(data);
    uint64_t key_quarter_holder_number = dead_relay->holders.key_quarter_holders[key_quarter_holder_position];
    return data.relay_state->GetRelayByNumber(key_quarter_holder_number);
}
