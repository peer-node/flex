#include "KeyDistributionAuditComplaint.h"
#include "KeyDistributionAuditMessage.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"

Point KeyDistributionAuditComplaint::VerificationKey(Data data)
{
    auto auditor = GetAuditor(data);
    if (auditor == NULL)
        return Point(CBigNum(0));
    return auditor->public_signing_key;
}

uint64_t KeyDistributionAuditComplaint::GetAuditorNumber(Data data)
{
    KeyDistributionAuditMessage audit_message = data.GetMessage(audit_message_hash);
    auto auditor_selection = audit_message.GetAuditorSelection(data);

    if (set_of_encrypted_key_twentyfourths == 1)
        return auditor_selection.first_set_of_auditors[position_of_bad_encrypted_key_twentyfourth];
    else if (set_of_encrypted_key_twentyfourths == 2)
        return auditor_selection.second_set_of_auditors[position_of_bad_encrypted_key_twentyfourth];

    return 0;
}

Relay *KeyDistributionAuditComplaint::GetAuditor(Data data)
{
    return data.relay_state->GetRelayByNumber(GetAuditorNumber(data));
}

Relay *KeyDistributionAuditComplaint::GetKeySharer(Data data)
{
    KeyDistributionAuditMessage audit_message = data.GetMessage(audit_message_hash);
    return data.relay_state->GetRelayByNumber(audit_message.relay_number);
}

Point KeyDistributionAuditComplaint::KeySharersPublicKeyTwentyFourth(Data data)
{
    auto key_sharer = GetKeySharer(data);
    return key_sharer->PublicKeyTwentyFourths()[position_of_bad_encrypted_key_twentyfourth];
}

void KeyDistributionAuditComplaint::PopulatePrivateReceivingKey(Data data)
{
    auto auditor = GetAuditor(data);
    Point public_key_twentyfourth = KeySharersPublicKeyTwentyFourth(data);
    private_receiving_key = auditor->GenerateRecipientPrivateKey(public_key_twentyfourth, data).getuint256();
}

uint256 KeyDistributionAuditComplaint::GetReportedBadEncryptedSecret(Data data)
{
    KeyDistributionAuditMessage audit_message = data.GetMessage(audit_message_hash);
    if (set_of_encrypted_key_twentyfourths == 1)
        return audit_message.first_set_of_encrypted_key_twentyfourths[position_of_bad_encrypted_key_twentyfourth];
    else
        return audit_message.second_set_of_encrypted_key_twentyfourths[position_of_bad_encrypted_key_twentyfourth];
}

bool KeyDistributionAuditComplaint::VerifyEncryptedSecretWasBad(Data data)
{
    uint256 encrypted_secret = GetReportedBadEncryptedSecret(data);
    Point point_corresponding_to_secret = KeySharersPublicKeyTwentyFourth(data);
    CBigNum shared_secret = Hash(private_receiving_key * point_corresponding_to_secret);
    CBigNum decrypted_secret = CBigNum(encrypted_secret) ^ shared_secret;
    return Point(decrypted_secret) != point_corresponding_to_secret;
}

bool KeyDistributionAuditComplaint::VerifyPrivateReceivingKeyIsCorrect(Data data)
{
    auto auditor = GetAuditor(data);
    Point public_key_twentyfourth = KeySharersPublicKeyTwentyFourth(data);
    Point public_receiving_key = auditor->GenerateRecipientPublicKey(public_key_twentyfourth);
    return Point(CBigNum(private_receiving_key)) == public_receiving_key;
}
