#ifndef TELEPORT_KEYDISTRIBUTIONAUDITFAILUREMESSAGE_H
#define TELEPORT_KEYDISTRIBUTIONAUDITFAILUREMESSAGE_H


#include <src/crypto/signature.h>
#include "test/teleport_tests/node/Data.h"

class Relay;

class KeyDistributionAuditFailureMessage
{
public:
    uint160 audit_message_hash{0};
    uint160 key_distribution_message_hash{0};
    uint64_t position_of_bad_secret{0};
    uint8_t first_or_second_auditor{0};
    uint256 private_key_twentyfourth{0};
    Signature signature;

    static std::string Type() { return "key_distribution_audit_failure"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(audit_message_hash);
        READWRITE(key_distribution_message_hash);
        READWRITE(position_of_bad_secret);
        READWRITE(first_or_second_auditor);
        READWRITE(private_key_twentyfourth);
        READWRITE(signature);
    );

    JSON(audit_message_hash, key_distribution_message_hash, position_of_bad_secret, first_or_second_auditor,
         private_key_twentyfourth, signature);

    DEPENDENCIES(audit_message_hash, key_distribution_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    bool VerifyEnclosedPrivateKeyTwentyFourthIsCorrect(Data data);

    Relay *GetKeySharer(Data data);

    bool VerifyKeyTwentyFourthInKeyDistributionMessageIsIncorrect(Data data);
};


#endif //TELEPORT_KEYDISTRIBUTIONAUDITFAILUREMESSAGE_H
