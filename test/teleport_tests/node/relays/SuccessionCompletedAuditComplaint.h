#ifndef TELEPORT_SUCCESSIONCOMPLETEDAUDITCOMPLAINT_H
#define TELEPORT_SUCCESSIONCOMPLETEDAUDITCOMPLAINT_H


#include <src/crypto/signature.h>
#include "test/teleport_tests/node/Data.h"

class SuccessionCompletedAuditComplaint
{
public:
    uint160 succession_completed_audit_message_hash{0};
    uint160 succession_completed_message_hash{0};
    uint64_t successor_number{0};
    uint64_t dead_relay_number{0};
    uint64_t auditor_number{0};
    uint8_t first_or_second_set_of_secrets{0};
    uint32_t position_of_bad_secret{0};
    uint256 private_receiving_key{0};
    Signature signature;

    static std::string Type() { return "succession_completed_audit_complaint"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(succession_completed_audit_message_hash);
        READWRITE(succession_completed_message_hash);
        READWRITE(successor_number);
        READWRITE(dead_relay_number);
        READWRITE(auditor_number);
        READWRITE(first_or_second_set_of_secrets);
        READWRITE(position_of_bad_secret);
        READWRITE(private_receiving_key);
        READWRITE(signature);
    );

    JSON(succession_completed_audit_message_hash, succession_completed_message_hash, successor_number,
         dead_relay_number, auditor_number, first_or_second_set_of_secrets,
         position_of_bad_secret, private_receiving_key, signature);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data)
    {
        return Point(CBigNum(0));
    }
};


#endif //TELEPORT_SUCCESSIONCOMPLETEDAUDITCOMPLAINT_H
