#include "KeyDistributionComplaint.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "KeyDistributionComplaint.cpp"

Relay *KeyDistributionComplaint::GetComplainer(Data data)
{
    Relay *sender = GetSecretSender(data);
    if (sender == NULL)
        return NULL;

    if (set_of_secrets == KEY_DISTRIBUTION_COMPLAINT_KEY_QUARTERS)
        return data.relay_state->GetRelayByNumber(sender->key_quarter_holders[position_of_secret / 4]);
    else if (set_of_secrets == KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS)
        return data.relay_state->GetRelayByNumber(
                sender->first_set_of_key_sixteenth_holders[position_of_secret]);
    else if (set_of_secrets == KEY_DISTRIBUTION_COMPLAINT_SECOND_KEY_SIXTEENTHS)
        return data.relay_state->GetRelayByNumber(
                sender->second_set_of_key_sixteenth_holders[position_of_secret]);

    return NULL;
}

Point KeyDistributionComplaint::VerificationKey(Data data)
{
    Relay *complainer = GetComplainer(data);

    if (complainer == NULL)
        return Point(SECP256K1, 0);

    return complainer->public_signing_key;
}

Relay *KeyDistributionComplaint::GetSecretSender(Data data)
{
    auto key_distribution_message = GetKeyDistributionMessage(data);
    return data.relay_state->GetRelayByJoinHash(key_distribution_message.relay_join_hash);
}

Point KeyDistributionComplaint::GetPointCorrespondingToSecret(Data data)
{
    Relay *secret_sender = GetSecretSender(data);
    if (secret_sender == NULL)
        throw RelayStateException("GetPointCorrespondingToSecret: no such relay");
    return secret_sender->PublicKeySixteenths()[position_of_secret];
}

CBigNum KeyDistributionComplaint::GetEncryptedSecret(Data data)
{
    auto message = GetKeyDistributionMessage(data);

    if (set_of_secrets == KEY_DISTRIBUTION_COMPLAINT_KEY_QUARTERS)
        return message.key_sixteenths_encrypted_for_key_quarter_holders[position_of_secret];
    if (set_of_secrets == KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS)
        return message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[position_of_secret];
    if (set_of_secrets == KEY_DISTRIBUTION_COMPLAINT_SECOND_KEY_SIXTEENTHS)
        return message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[position_of_secret];

    return 0;
}

KeyDistributionMessage KeyDistributionComplaint::GetKeyDistributionMessage(Data data)
{
    return data.msgdata[key_distribution_message_hash]["key_distribution"];
}

KeyDistributionComplaint::KeyDistributionComplaint(KeyDistributionMessage key_distribution_message,
                                                   uint64_t set_of_secrets, uint64_t position_of_secret, Data data):
    key_distribution_message_hash(key_distribution_message.GetHash160()),
    set_of_secrets(set_of_secrets),
    position_of_secret(position_of_secret)
{
    Relay *recipient = GetComplainer(data);
    recipient_private_key = recipient->GenerateRecipientPrivateKey(GetPointCorrespondingToSecret(data), data);
}

KeyDistributionComplaint::KeyDistributionComplaint(uint160 key_distribution_message_hash,
                                                   uint64_t set_of_secrets, uint64_t position_of_secret, Data data):
        key_distribution_message_hash(key_distribution_message_hash),
        set_of_secrets(set_of_secrets),
        position_of_secret(position_of_secret)
{
    Relay *recipient = GetComplainer(data);
    recipient_private_key = recipient->GenerateRecipientPrivateKey(GetPointCorrespondingToSecret(data), data);
}

bool KeyDistributionComplaint::IsValid(Data data)
{
    return not (EncryptedSecretIsOk(data) and GeneratedRowOfPointsIsOk(data));
}

bool KeyDistributionComplaint::EncryptedSecretIsOk(Data data)
{
    auto recovered_secret = RecoverSecret(data);
    return GetPointCorrespondingToSecret(data) == Point(recovered_secret);
}

CBigNum KeyDistributionComplaint::RecoverSecret(Data data)
{
    CBigNum shared_secret = Hash(recipient_private_key * GetPointCorrespondingToSecret(data));
    return shared_secret ^ GetEncryptedSecret(data);
}

bool KeyDistributionComplaint::GeneratedRowOfPointsIsOk(Data data)
{
    MemoryDataStore tmp_keydata;
    auto secret = RecoverSecret(data);
    tmp_keydata[Point(secret)]["privkey"] = secret;
    Relay *key_sharer = GetSecretSender(data);
    return key_sharer->public_key_set.VerifyRowOfGeneratedPoints(position_of_secret, tmp_keydata);
}