#ifndef TELEPORT_KEYDISTRIBUTIONAUDITCOMPLAINT_H
#define TELEPORT_KEYDISTRIBUTIONAUDITCOMPLAINT_H


#include <src/crypto/signature.h>
#include "test/teleport_tests/node/Data.h"

class Relay;

class KeyDistributionAuditComplaint
{
public:
    uint160 audit_message_hash{0};
    uint8_t set_of_encrypted_key_twentyfourths{0};
    uint64_t position_of_bad_encrypted_key_twentyfourth{0};
    uint256 private_receiving_key{0};
    Signature signature;

    static std::string Type() { return "key_distribution_audit_complaint"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(audit_message_hash);
        READWRITE(set_of_encrypted_key_twentyfourths);
        READWRITE(position_of_bad_encrypted_key_twentyfourth);
        READWRITE(private_receiving_key);
        READWRITE(signature);
    );

    JSON(audit_message_hash, set_of_encrypted_key_twentyfourths, position_of_bad_encrypted_key_twentyfourth,
         private_receiving_key, signature);

    DEPENDENCIES(audit_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    uint64_t GetAuditorNumber(Data data);

    Relay *GetAuditor(Data data);

    void PopulatePrivateReceivingKey(Data data);

    Relay *GetKeySharer(Data data);

    bool VerifyEncryptedSecretWasBad(Data data);

    Point KeySharersPublicKeyTwentyFourth(Data data);

    uint256 GetReportedBadEncryptedSecret(Data data);

    bool VerifyPrivateReceivingKeyIsCorrect(Data data);
};


#endif //TELEPORT_KEYDISTRIBUTIONAUDITCOMPLAINT_H
