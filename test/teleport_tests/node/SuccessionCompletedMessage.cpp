#include "SuccessionCompletedMessage.h"
#include "relay_handler/RelayState.h"


Point SuccessionCompletedMessage::VerificationKey(Data data)
{
    auto successor = data.relay_state->GetRelayByNumber(successor_number);
    if (successor == NULL)
        return Point(CBigNum(0));
    return successor->public_signing_key;
}

bool SuccessionCompletedMessage::IsValid(Data data)
{
    if (goodbye_message_hash != 0)
        return IsValidSuccessionFromGoodbyeMessage(data);
    else
        return IsValidSuccessionFromSecretRecoveryMessages(data);
}

bool SuccessionCompletedMessage::IsValidSuccessionFromGoodbyeMessage(Data data)
{
    GoodbyeMessage goodbye_message = data.GetMessage(goodbye_message_hash);
    if (goodbye_message.dead_relay_number != dead_relay_number)
        return false;
    if (goodbye_message.successor_number != successor_number)
        return false;;
    return true;
}

bool SuccessionCompletedMessage::IsValidSuccessionFromSecretRecoveryMessages(Data data)
{
    if (recovery_message_hashes.size() != 4)
        return false;
    SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hashes[0]);
    if (successor_number != recovery_message.successor_number)
        return false;
    if (dead_relay_number != recovery_message.dead_relay_number)
        return false;;
    return true;
}