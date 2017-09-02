
#ifndef TELEPORT_SUCCESSIONCOMPLETEDMESSAGE_H
#define TELEPORT_SUCCESSIONCOMPLETEDMESSAGE_H


#include <src/crypto/signature.h>
#include "test/teleport_tests/node/Data.h"
#include "FourSecretRecoveryMessages.h"
#include "test/teleport_tests/node/relays/structures/EncryptedKeyQuarter.h"

class SuccessionCompletedMessage
{
public:
    std::array<uint160, 4> recovery_message_hashes;
    uint64_t dead_relay_number{0};
    uint64_t successor_number{0};
    std::vector<EncryptedKeyQuarter> encrypted_key_quarters;
    std::vector<uint64_t> key_quarter_sharers;
    std::vector<uint8_t> key_quarter_positions;
    Signature signature;

    static std::string Type() { return "succession_completed"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(recovery_message_hashes);
        READWRITE(dead_relay_number);
        READWRITE(successor_number);
        READWRITE(encrypted_key_quarters);
        READWRITE(key_quarter_sharers);
        READWRITE(key_quarter_positions);
        READWRITE(signature);
    );

    JSON(recovery_message_hashes, dead_relay_number, successor_number, encrypted_key_quarters,
         key_quarter_sharers, key_quarter_positions, signature);

    IMPLEMENT_HASH_SIGN_VERIFY();

    FourSecretRecoveryMessages GetFourSecretRecoveryMessages()
    {
        FourSecretRecoveryMessages four_messages;
        four_messages.secret_recovery_messages = recovery_message_hashes;
        return four_messages;
    }

    std::vector<uint160> Dependencies()
    {
        std::vector<uint160> dependencies;
        for (uint32_t i = 0; i < 4; i++)
            dependencies.push_back(recovery_message_hashes[i]);
        return dependencies;
    }

    Point VerificationKey(Data data);

    bool IsValid(Data data);
};


#endif //TELEPORT_SUCCESSIONCOMPLETEDMESSAGE_H
