#include "KeyDistributionAuditMessage.h"
#include "test/teleport_tests/node/relays/structures/RelayMemoryCache.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"

using std::vector;

#include "log.h"
#define LOG_CATEGORY "KeyDistributionAuditMessage.cpp"

#define NO_BAD_ENCLOSED_SECRET_FOUND std::pair<uint8_t, uint64_t>(0, 0)
#define NO_BAD_SECRET_FOUND_IN_KEY_DISTRIBUTION_MESSAGE -1

Point KeyDistributionAuditMessage::VerificationKey(Data data)
{
    auto key_sharer = data.relay_state->GetRelayByNumber(relay_number);
    if (key_sharer == NULL)
        return Point(CBigNum(0));
    return key_sharer->public_signing_key;
}

KeyDistributionAuditorSelection KeyDistributionAuditMessage::GetAuditorSelection(Data data)
{
    if (cached_auditor_selection.size() == 0)
    {
        auto &relay_state = RelayStateFromWhichAuditorsAreChosen(data);
        auto relay = relay_state.GetRelayByNumber(relay_number);
        KeyDistributionAuditorSelection auditor_selection;
        auditor_selection.Populate(relay, &relay_state);
        cached_auditor_selection.push_back(auditor_selection);
    }
    return cached_auditor_selection[0];
}

RelayState& KeyDistributionAuditMessage::RelayStateFromWhichAuditorsAreChosen(Data data)
{
    if (cached_relay_state.size() == 0)
    {
        MinedCreditMessage msg = data.GetMessage(hash_of_message_containing_relay_state);
        RelayState relay_state = data.cache->RetrieveRelayState(msg.mined_credit.network_state.relay_state_hash);
        relay_state.latest_mined_credit_message_hash = hash_of_message_containing_relay_state;
        cached_relay_state.push_back(relay_state);
    }
    return cached_relay_state[0];
}

void KeyDistributionAuditMessage::Populate(Relay *key_sharer, uint160 hash_of_message_containing_relay_state_, Data data)
{
    relay_number = key_sharer->number;
    hash_of_message_containing_relay_state = hash_of_message_containing_relay_state_;
    key_distribution_message_hash = key_sharer->key_quarter_locations[0].message_hash;
    PopulateEncryptedSecrets(key_sharer, data);
}

void KeyDistributionAuditMessage::PopulateEncryptedSecrets(Relay *key_sharer, Data data)
{
    for (uint64_t position = 0; position < 24; position++)
    {
        auto secret = key_sharer->PrivateKeyTwentyFourths(data.keydata)[position];
        auto first_auditor = GetSpecifiedAuditor(1, position, data);
        first_set_of_encrypted_key_twentyfourths[position] = first_auditor->EncryptSecret(secret);
        auto second_auditor = GetSpecifiedAuditor(2, position, data);
        second_set_of_encrypted_key_twentyfourths[position] = second_auditor->EncryptSecret(secret);
    }
}

bool KeyDistributionAuditMessage::EnclosedSecretsAreCorrect(Data data)
{
    return GetCoordinatesOfBadEnclosedSecret(data) == NO_BAD_ENCLOSED_SECRET_FOUND;
}

Point KeyDistributionAuditMessage::GetPublicKeyTwentyFourth(uint64_t position, Data data)
{
    auto key_sharer = data.relay_state->GetRelayByNumber(relay_number);
    return key_sharer->PublicKeyTwentyFourths()[position];
}

CBigNum KeyDistributionAuditMessage::GetDecryptedSecret(uint256 encrypted_secret, uint64_t position,
                                                        Relay *auditor, Data data)
{
    auto public_key_twentyfourth = GetPublicKeyTwentyFourth(position, data);
    return auditor->DecryptSecret(encrypted_secret, public_key_twentyfourth, data);
}

bool KeyDistributionAuditMessage::EnclosedSecretIsCorrect(uint256 encrypted_secret, uint64_t position,
                                                          Relay *auditor, Data data)
{
    auto public_key_twentyfourth = GetPublicKeyTwentyFourth(position, data);
    auto decrypted_secret = auditor->DecryptSecret(encrypted_secret, public_key_twentyfourth, data);
    return Point(decrypted_secret) == public_key_twentyfourth;
}

KeyDistributionAuditComplaint KeyDistributionAuditMessage::GenerateComplaint(Data data)
{
    auto coordinates_of_bad_secret = GetCoordinatesOfBadEnclosedSecret(data);
    KeyDistributionAuditComplaint complaint;
    complaint.audit_message_hash = GetHash160();
    complaint.set_of_encrypted_key_twentyfourths = coordinates_of_bad_secret.first;
    complaint.position_of_bad_encrypted_key_twentyfourth = coordinates_of_bad_secret.second;
    complaint.PopulatePrivateReceivingKey(data);
    return complaint;
}

std::vector<std::array<uint256, 24> > KeyDistributionAuditMessage::EnclosedEncryptedSecrets()
{
    return std::vector<std::array<uint256, 24> >{first_set_of_encrypted_key_twentyfourths,
                                                 second_set_of_encrypted_key_twentyfourths};
}

std::vector<std::array<uint64_t, 24> > KeyDistributionAuditMessage::GetAuditors(Data data)
{
    auto auditor_selection = GetAuditorSelection(data);
    return std::vector<std::array<uint64_t, 24> >{auditor_selection.first_set_of_auditors,
                                                  auditor_selection.second_set_of_auditors};
}

std::pair<uint8_t, uint64_t> KeyDistributionAuditMessage::GetCoordinatesOfBadEnclosedSecret(Data data)
{
    for (uint8_t first_or_second_set = 1; first_or_second_set <= 2; first_or_second_set++)
        for (uint64_t position = 0; position < 24; position++)
        {
            auto auditor = GetSpecifiedAuditor(first_or_second_set, position, data);
            auto encrypted_secret = EnclosedEncryptedSecret(first_or_second_set, position);
            if (auditor->PrivateKeyIsPresent(data) and not EnclosedSecretIsCorrect(encrypted_secret,
                                                                                   position, auditor, data))
                return std::pair<uint8_t, uint64_t>(first_or_second_set, position);
        }
    return NO_BAD_ENCLOSED_SECRET_FOUND;
}

uint256 KeyDistributionAuditMessage::EnclosedEncryptedSecret(uint64_t first_or_second_set, uint64_t position)
{
    auto encrypted_secrets = EnclosedEncryptedSecrets();
    return encrypted_secrets[first_or_second_set - 1][position];
}

KeyDistributionMessage &KeyDistributionAuditMessage::GetKeyDistributionMessage(Data data)
{
    if (cached_key_distribution_message.size() == 0)
    {
        KeyDistributionMessage distribution_message = data.GetMessage(key_distribution_message_hash);
        cached_key_distribution_message.push_back(distribution_message);
    }
    return cached_key_distribution_message[0];
}

uint256 KeyDistributionAuditMessage::EncryptedKeyTwentyFourth(uint64_t position, Data data)
{
    auto &distribution_message = GetKeyDistributionMessage(data);
    return distribution_message.EncryptedKeyTwentyFourths()[position];
}

Relay *KeyDistributionAuditMessage::GetQuarterHolderWhoReceivedSecretAtPosition(uint64_t position, Data data)
{
    auto &relay_state = RelayStateFromWhichAuditorsAreChosen(data);
    auto recipients = relay_state.GetKeyQuarterHoldersAsListOf24RelayNumbers(relay_number);
    return relay_state.GetRelayByNumber(recipients[position]);
}

Relay *KeyDistributionAuditMessage::GetSpecifiedAuditor(uint32_t first_or_second_set, uint64_t position, Data data)
{
    auto &relay_state = RelayStateFromWhichAuditorsAreChosen(data);
    auto auditors = GetAuditors(data);
    auto auditor = relay_state.GetRelayByNumber(auditors[first_or_second_set - 1][position]);

    return auditor;
}

bool KeyDistributionAuditMessage::VerifySecretsInKeyDistributionMessageAreCorrect(Data data)
{
    return GetPositionOfBadSecretInKeyDistributionMessage(data) == NO_BAD_SECRET_FOUND_IN_KEY_DISTRIBUTION_MESSAGE;
}

int64_t KeyDistributionAuditMessage::GetPositionOfBadSecretInKeyDistributionMessage(Data data)
{
    for (uint8_t first_or_second_set = 1; first_or_second_set <= 2; first_or_second_set++)
        for (uint64_t position = 0; position < 24; position++)
        {
            auto auditor = GetSpecifiedAuditor(first_or_second_set, position, data);
            auto encrypted_secret = EnclosedEncryptedSecret(first_or_second_set, position);
            auto quarter_holder = GetQuarterHolderWhoReceivedSecretAtPosition(position, data);

            if (auditor->PrivateKeyIsPresent(data) and EnclosedSecretIsCorrect(encrypted_secret, position, auditor, data))
            {
                CBigNum private_key_twentyfourth = GetDecryptedSecret(encrypted_secret, position, auditor, data);

                if (EncryptedKeyTwentyFourth(position, data) != quarter_holder->EncryptSecret(private_key_twentyfourth))
                    return position;
            }
        }
    return NO_BAD_SECRET_FOUND_IN_KEY_DISTRIBUTION_MESSAGE;
}

CBigNum KeyDistributionAuditMessage::GetDecryptedSecret(uint64_t position, Data data)
{
    auto first_auditor = GetSpecifiedAuditor(1, position, data);
    auto second_auditor = GetSpecifiedAuditor(2, position, data);
    auto auditor = first_auditor->PrivateKeyIsPresent(data)? first_auditor : second_auditor;
    auto encrypted_secret = EnclosedEncryptedSecret(first_auditor->PrivateKeyIsPresent(data) ? 1 : 2, position);
    return GetDecryptedSecret(encrypted_secret, position, auditor, data);
}

KeyDistributionAuditFailureMessage KeyDistributionAuditMessage::GenerateAuditFailureMessage(Data data)
{
    uint64_t position = (uint64_t)GetPositionOfBadSecretInKeyDistributionMessage(data);

    auto first_auditor = GetSpecifiedAuditor(1, position, data);

    KeyDistributionAuditFailureMessage failure_message;
    failure_message.position_of_bad_secret = position;
    failure_message.audit_message_hash = GetHash160();
    failure_message.first_or_second_auditor = uint8_t(first_auditor->PrivateKeyIsPresent(data) ? 1 : 2);
    failure_message.key_distribution_message_hash = key_distribution_message_hash;
    failure_message.private_key_twentyfourth = GetDecryptedSecret(position, data).getuint256();

    return failure_message;
}
