#include "KeyDistributionAuditFailureMessage.h"
#include "KeyDistributionAuditMessage.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "KeyDistributionAuditFailureMessage.cpp"

Point KeyDistributionAuditFailureMessage::VerificationKey(Data data)
{
    KeyDistributionAuditMessage audit_message = data.GetMessage(audit_message_hash);
    auto auditors = audit_message.GetAuditors(data);
    auto auditor_number = auditors[first_or_second_auditor - 1][position_of_bad_secret];
    auto auditor = data.relay_state->GetRelayByNumber(auditor_number);
    return auditor->public_signing_key;
}

Relay *KeyDistributionAuditFailureMessage::GetKeySharer(Data data)
{
    KeyDistributionAuditMessage audit_message = data.GetMessage(audit_message_hash);
    return data.relay_state->GetRelayByNumber(audit_message.relay_number);
}

bool KeyDistributionAuditFailureMessage::VerifyEnclosedPrivateKeyTwentyFourthIsCorrect(Data data)
{
    auto key_sharer = GetKeySharer(data);
    auto public_key_twentyfourth = key_sharer->PublicKeyTwentyFourths()[position_of_bad_secret];
    return Point(CBigNum(private_key_twentyfourth)) == public_key_twentyfourth;
}

bool KeyDistributionAuditFailureMessage::VerifyKeyTwentyFourthInKeyDistributionMessageIsIncorrect(Data data)
{
    KeyDistributionMessage key_distribution_message = data.GetMessage(key_distribution_message_hash);

    auto key_sharer = GetKeySharer(data);
    auto quarter_holder = key_sharer->QuarterHolders(data)[position_of_bad_secret / 6];
    auto correctly_encrypted_key_twentyfourth = quarter_holder->EncryptSecret(CBigNum(private_key_twentyfourth));

    return key_distribution_message.EncryptedKeyTwentyFourths()[position_of_bad_secret]
            != correctly_encrypted_key_twentyfourth;
}
