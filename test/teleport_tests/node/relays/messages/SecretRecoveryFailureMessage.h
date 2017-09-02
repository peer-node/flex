#ifndef TELEPORT_SECRETRECOVERYFAILUREMESSAGE_H
#define TELEPORT_SECRETRECOVERYFAILUREMESSAGE_H

#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include <src/crypto/point.h>
#include <src/crypto/signature.h>
#include "test/teleport_tests/node/Data.h"


class Relay;


class SecretRecoveryFailureMessage
{
public:
    uint64_t dead_relay_number{0};
    uint64_t successor_number{0};
    uint64_t key_sharer_position{0};
    std::array<uint160, 4> recovery_message_hashes{{0,0,0,0}};
    uint64_t shared_secret_quarter_position{0};
    Point sum_of_decrypted_shared_secret_quarters;
    Signature signature;

    static std::string Type() { return "secret_recovery_failure"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(dead_relay_number);
        READWRITE(successor_number);
        READWRITE(recovery_message_hashes);
        READWRITE(key_sharer_position);
        READWRITE(shared_secret_quarter_position);
        READWRITE(sum_of_decrypted_shared_secret_quarters);
        READWRITE(signature);
    );

    JSON(dead_relay_number, successor_number, recovery_message_hashes, key_sharer_position,
         shared_secret_quarter_position,
         sum_of_decrypted_shared_secret_quarters, signature);

    std::vector<uint160> Dependencies()
    {
        std::vector<uint160> dependencies;
        for (auto hash : recovery_message_hashes)
            dependencies.push_back(hash);
        return dependencies;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    void Populate(std::array<uint160, 4> recovery_message_hashes, Data data);

    void PopulateDetailsOfFailedSharedSecret(Data data);

    Relay *GetDeadRelay(Data data);

    bool ValidateSizes();

    bool IsValid(Data data);

    std::vector<Relay *> GetQuarterHolders(Data data);

    Point GetKeyTwentyFourth(Data data);

    Relay *GetKeySharer(Data data);

    uint64_t GetKeyQuarterPosition(Data data);
};


#endif //TELEPORT_SECRETRECOVERYFAILUREMESSAGE_H
