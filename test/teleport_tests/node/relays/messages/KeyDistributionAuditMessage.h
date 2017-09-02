#ifndef TELEPORT_KEYDISTRIBUTIONAUDITMESSAGE_H
#define TELEPORT_KEYDISTRIBUTIONAUDITMESSAGE_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include <src/crypto/signature.h>
#include "test/teleport_tests/node/Data.h"
#include "test/teleport_tests/node/relays/structures/KeyDistributionAuditorSelection.h"
#include "KeyDistributionAuditComplaint.h"
#include "KeyDistributionAuditFailureMessage.h"
#include <array>


class KeyDistributionAuditMessage
{
public:
    uint64_t relay_number{0};
    uint160 key_distribution_message_hash{0};
    uint160 hash_of_message_containing_relay_state{0};
    std::array<uint256, 24> first_set_of_encrypted_key_twentyfourths;
    std::array<uint256, 24> second_set_of_encrypted_key_twentyfourths;
    uint8_t position{ALL_POSITIONS};
    Signature signature;

    static std::string Type() { return "key_distribution_audit"; }

    KeyDistributionAuditMessage &operator=(const KeyDistributionAuditMessage& other)
    {
        relay_number = other.relay_number;
        key_distribution_message_hash = other.key_distribution_message_hash;
        hash_of_message_containing_relay_state = other.hash_of_message_containing_relay_state;
        first_set_of_encrypted_key_twentyfourths = other.first_set_of_encrypted_key_twentyfourths;
        second_set_of_encrypted_key_twentyfourths = other.second_set_of_encrypted_key_twentyfourths;
        position = other.position;
        signature = other.signature;
        return *this;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relay_number);
        READWRITE(key_distribution_message_hash);
        READWRITE(hash_of_message_containing_relay_state);
        READWRITE(first_set_of_encrypted_key_twentyfourths);
        READWRITE(second_set_of_encrypted_key_twentyfourths);
        READWRITE(position);
        READWRITE(signature);
    );

    JSON(relay_number, key_distribution_message_hash, hash_of_message_containing_relay_state,
         first_set_of_encrypted_key_twentyfourths, second_set_of_encrypted_key_twentyfourths, position, signature);

    DEPENDENCIES(key_distribution_message_hash, hash_of_message_containing_relay_state);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    KeyDistributionAuditorSelection GetAuditorSelection(Data data);

    void Populate(Relay *key_sharer, uint160 hash_of_message_containing_relay_state, Data data);

    void PopulateEncryptedSecrets(Relay *key_sharer, Data data);

    RelayState& RelayStateFromWhichAuditorsAreChosen(Data data);

    bool VerifySecretsInKeyDistributionMessageAreCorrect(Data data);

    bool EnclosedSecretsAreCorrect(Data data);

    bool EnclosedSecretIsCorrect(uint256 encrypted_secret, uint64_t position, Relay *auditor, Data data);

    KeyDistributionAuditComplaint GenerateComplaint(Data data);

    std::pair<uint8_t, uint64_t> GetCoordinatesOfBadEnclosedSecret(Data data);

    std::vector<std::array<uint256, 24>> EnclosedEncryptedSecrets();

    std::vector<std::array<uint64_t, 24>> GetAuditors(Data data);

    CBigNum GetDecryptedSecret(uint256 encrypted_secret, uint64_t position, Relay *auditor, Data data);

    Point GetPublicKeyTwentyFourth(uint64_t position, Data data);

    KeyDistributionAuditFailureMessage GenerateAuditFailureMessage(Data data);

    int64_t GetPositionOfBadSecretInKeyDistributionMessage(Data data);

    uint256 EnclosedEncryptedSecret(uint64_t first_or_second_set, uint64_t position);

    uint256 EncryptedKeyTwentyFourth(uint64_t position, Data data);

private:
    std::vector<KeyDistributionMessage> cached_key_distribution_message;
    std::vector<RelayState> cached_relay_state;
    std::vector<KeyDistributionAuditorSelection> cached_auditor_selection;

    KeyDistributionMessage &GetKeyDistributionMessage(Data data);

    CBigNum GetDecryptedSecret(uint64_t position, Data data);

    Relay *GetQuarterHolderWhoReceivedSecretAtPosition(uint64_t position, Data data);

    Relay *GetSpecifiedAuditor(uint32_t first_of_second_set, uint64_t position, Data data);
};


#endif //TELEPORT_KEYDISTRIBUTIONAUDITMESSAGE_H
