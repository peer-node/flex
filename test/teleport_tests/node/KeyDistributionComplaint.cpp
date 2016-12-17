#include "KeyDistributionComplaint.h"
#include "RelayState.h"

Relay *KeyDistributionComplaint::GetComplainer(Data data)
{
    Relay *sender = GetSecretSender(data);
    if (sender == NULL)
        return NULL;

    if (set_of_secrets == 0)
        return data.relay_state->GetRelayByNumber(sender->key_quarter_holders[position_of_secret]);
    else if (set_of_secrets == 1)
        return data.relay_state->GetRelayByNumber(
                sender->first_set_of_key_sixteenth_holders[position_of_secret]);
    else if (set_of_secrets == 2)
        return data.relay_state->GetRelayByNumber(
                sender->second_set_of_key_sixteenth_holders[position_of_secret]);

    return NULL;
}

Point KeyDistributionComplaint::VerificationKey(Data data)
{
    Relay *complainer = GetComplainer(data);

    if (complainer == NULL)
        return Point(SECP256K1, 0);

    return complainer->public_key;
}

Relay *KeyDistributionComplaint::GetSecretSender(Data data)
{
    KeyDistributionMessage key_distribution_message = data.msgdata[key_distribution_message_hash]["key_distribution"];
    return data.relay_state->GetRelayByJoinHash(key_distribution_message.relay_join_hash);
}

KeyDistributionComplaint::KeyDistributionComplaint(KeyDistributionMessage key_distribution_message,
                                                   uint64_t set_of_secrets, uint64_t position_of_secret):
    key_distribution_message_hash(key_distribution_message.GetHash160()), set_of_secrets(set_of_secrets),
    position_of_secret(position_of_secret)
{

}
