
#ifndef TELEPORT_SUCCESSIONCOMPLETEDMESSAGE_H
#define TELEPORT_SUCCESSIONCOMPLETEDMESSAGE_H


#include <src/crypto/signature.h>
#include "Data.h"

class SuccessionCompletedMessage
{
public:
    uint160 goodbye_message_hash{0};
    std::vector<uint160> recovery_message_hashes;
    uint64_t dead_relay_number{0};
    uint64_t successor_number{0};
    Signature signature;

    static std::string Type() { return "succession_completed"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(goodbye_message_hash);
        READWRITE(recovery_message_hashes);
        READWRITE(dead_relay_number);
        READWRITE(successor_number);
        READWRITE(signature);
    );

    JSON(goodbye_message_hash, recovery_message_hashes, dead_relay_number, successor_number, signature);

    IMPLEMENT_HASH_SIGN_VERIFY();

    std::vector<uint160> Dependencies()
    {
        if (goodbye_message_hash == 0)
            return recovery_message_hashes;
        return std::vector<uint160>{goodbye_message_hash};
    }

    Point VerificationKey(Data data);

    bool IsValid(Data data);

    bool IsValidSuccessionFromGoodbyeMessage(Data data);

    bool IsValidSuccessionFromSecretRecoveryMessages(Data data);
};


#endif //TELEPORT_SUCCESSIONCOMPLETEDMESSAGE_H
