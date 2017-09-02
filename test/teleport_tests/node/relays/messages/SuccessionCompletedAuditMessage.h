#ifndef TELEPORT_SUCCESSIONCOMPLETEDAUDITMESSAGE_H
#define TELEPORT_SUCCESSIONCOMPLETEDAUDITMESSAGE_H


#include <src/crypto/uint256.h>
#include <src/crypto/signature.h>
#include <test/teleport_tests/node/Data.h>
#include "test/teleport_tests/node/relays/structures/EncryptedKeyQuarter.h"
#include "SuccessionCompletedMessage.h"

class SuccessionCompletedAuditMessage
{
public:
    uint64_t dead_relay_number{0};
    uint64_t successor_number{0};
    uint160 succession_completed_message_hash{0};
    uint160 encoding_message_hash{0};
    std::vector<EncryptedKeyQuarter> encrypted_key_quarters;
    Signature signature;

    static std::string Type() { return "succession_completed_audit"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(dead_relay_number);
        READWRITE(successor_number);
        READWRITE(succession_completed_message_hash);
        READWRITE(encoding_message_hash);
        READWRITE(encrypted_key_quarters);
        READWRITE(signature);
    )

    JSON(dead_relay_number, successor_number, succession_completed_message_hash,
         encoding_message_hash, encrypted_key_quarters, signature);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data)
    {
        SuccessionCompletedMessage succession_completed_message = data.GetMessage(succession_completed_message_hash);
        return succession_completed_message.VerificationKey(data);
    }
};


#endif //TELEPORT_SUCCESSIONCOMPLETEDAUDITMESSAGE_H
