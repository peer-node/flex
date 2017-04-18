#include "KeyDistributionComplaint.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "KeyDistributionComplaint.cpp"

Relay *KeyDistributionComplaint::GetComplainer(Data data)
{
    Relay *sender = GetSecretSender(data);
    if (sender == NULL)
        return NULL;

    return data.relay_state->GetRelayByNumber(sender->key_quarter_holders[position_of_secret / 6]);
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
    {
        log_ << "GetPointCorrespondingToSecret: no such relay\n";
        return Point(CBigNum(0));
    }
    return secret_sender->PublicKeyTwentyFourths()[position_of_secret];
}

uint256 KeyDistributionComplaint::GetEncryptedSecret(Data data)
{
    auto message = GetKeyDistributionMessage(data);
    return message.EncryptedKeyTwentyFourths()[position_of_secret];
}

KeyDistributionMessage KeyDistributionComplaint::GetKeyDistributionMessage(Data data)
{
    return data.msgdata[key_distribution_message_hash]["key_distribution"];
}

void KeyDistributionComplaint::Populate(uint160 key_distribution_message_hash_,
                                        uint64_t position_of_secret_, Data data)
{
    key_distribution_message_hash = key_distribution_message_hash_;
    position_of_secret = position_of_secret_;
    Relay *recipient = GetComplainer(data);
    Point corresponding_point = GetPointCorrespondingToSecret(data);
    recipient_private_key = recipient->GenerateRecipientPrivateKey(corresponding_point, data).getuint256();
}

bool KeyDistributionComplaint::IsValid(Data data)
{
    if (GetSecretSender(data) == NULL or GetComplainer(data) == NULL)
        return false;

    return ReferencedSecretExists(data) and RecipientPrivateKeyIsOk(data)
                                        and not (EncryptedSecretIsOk(data) and GeneratedRowOfPointsIsOk(data));
}

bool KeyDistributionComplaint::ReferencedSecretExists(Data data)
{
    return position_of_secret < 24;
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