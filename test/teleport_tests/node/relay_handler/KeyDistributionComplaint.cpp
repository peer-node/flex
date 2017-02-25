#include "KeyDistributionComplaint.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "KeyDistributionComplaint.cpp"

Relay *KeyDistributionComplaint::GetComplainer(Data data)
{
    Relay *sender = GetSecretSender(data);
    if (sender == NULL)
        return NULL;

    if (set_of_secrets == KEY_QUARTER_HOLDERS)
        return data.relay_state->GetRelayByNumber(sender->holders.key_quarter_holders[position_of_secret / 4]);
    else if (set_of_secrets == FIRST_SET_OF_KEY_SIXTEENTH_HOLDERS)
        return data.relay_state->GetRelayByNumber(
                sender->holders.first_set_of_key_sixteenth_holders[position_of_secret]);
    else if (set_of_secrets == SECOND_SET_OF_KEY_SIXTEENTH_HOLDERS)
        return data.relay_state->GetRelayByNumber(
                sender->holders.second_set_of_key_sixteenth_holders[position_of_secret]);

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
        throw RelayStateException("KeyDistributionComplaint::GetPointCorrespondingToSecret: no such relay");
    return secret_sender->PublicKeySixteenths()[position_of_secret];
}

uint256 KeyDistributionComplaint::GetEncryptedSecret(Data data)
{
    auto message = GetKeyDistributionMessage(data);

    if (set_of_secrets == KEY_QUARTER_HOLDERS)
        return message.key_sixteenths_encrypted_for_key_quarter_holders[position_of_secret];
    if (set_of_secrets == FIRST_SET_OF_KEY_SIXTEENTH_HOLDERS)
        return message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[position_of_secret];
    if (set_of_secrets == SECOND_SET_OF_KEY_SIXTEENTH_HOLDERS)
        return message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[position_of_secret];

    return 0;
}

KeyDistributionMessage KeyDistributionComplaint::GetKeyDistributionMessage(Data data)
{
    return data.msgdata[key_distribution_message_hash]["key_distribution"];
}

void KeyDistributionComplaint::Populate(uint160 key_distribution_message_hash_,
                                        uint64_t set_of_secrets_, uint64_t position_of_secret_, Data data)
{
    key_distribution_message_hash = key_distribution_message_hash_;
    set_of_secrets = set_of_secrets_;
    position_of_secret = position_of_secret_;
    Relay *recipient = GetComplainer(data);
    Point corresponding_point = GetPointCorrespondingToSecret(data);
    recipient_private_key = recipient->GenerateRecipientPrivateKey(corresponding_point, data).getuint256();
}

bool KeyDistributionComplaint::IsValid(Data data)
{
    if (GetSecretSender(data) == NULL or GetComplainer(data) == NULL)
        return false;

    if (DurationWithoutResponseHasElapsedSinceKeyDistributionMessage(data))
        return false;

    return ReferencedSecretExists(data) and RecipientPrivateKeyIsOk(data)
                                        and not (EncryptedSecretIsOk(data) and GeneratedRowOfPointsIsOk(data));
}

bool KeyDistributionComplaint::DurationWithoutResponseHasElapsedSinceKeyDistributionMessage(Data data)
{
    auto key_sharer = GetSecretSender(data);
    if (key_sharer == NULL)
    {
        log_ << "DurationWithoutResponseHasElapsedSinceKeyDistributionMessage: key sharer is null\n";
        return false;
    }
    return key_sharer->key_distribution_message_accepted;
}

bool KeyDistributionComplaint::ReferencedSecretExists(Data data)
{
    return position_of_secret < 16 and set_of_secrets <= SECOND_SET_OF_KEY_SIXTEENTH_HOLDERS;
}

bool KeyDistributionComplaint::RecipientPrivateKeyIsOk(Data data)
{
    auto recipient = GetComplainer(data);
    auto point_corresponding_to_secret = GetPointCorrespondingToSecret(data);
    return Point(CBigNum(recipient_private_key)) == recipient->GenerateRecipientPublicKey(point_corresponding_to_secret);
}

bool KeyDistributionComplaint::EncryptedSecretIsOk(Data data)
{
    auto recovered_secret = RecoverSecret(data);
    return GetPointCorrespondingToSecret(data) == Point(recovered_secret);
}

CBigNum KeyDistributionComplaint::RecoverSecret(Data data)
{
    CBigNum shared_secret = Hash(recipient_private_key * GetPointCorrespondingToSecret(data));
    return shared_secret ^ CBigNum(GetEncryptedSecret(data));
}

bool KeyDistributionComplaint::GeneratedRowOfPointsIsOk(Data data)
{
    MemoryDataStore tmp_keydata;
    auto secret = RecoverSecret(data);
    tmp_keydata[Point(secret)]["privkey"] = secret;
    Relay *key_sharer = GetSecretSender(data);
    return key_sharer->public_key_set.VerifyRowOfGeneratedPoints(position_of_secret, tmp_keydata);
}