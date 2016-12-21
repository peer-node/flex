#ifndef TELEPORT_SECRETRECOVERYMESSAGE_H
#define TELEPORT_SECRETRECOVERYMESSAGE_H


#include <src/crypto/bignum.h>
#include <src/crypto/signature.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "RelayJoinMessage.h"
#include "Data.h"
#include "Relay.h"


class SecretRecoveryMessage
{
public:
    uint160 obituary_hash{0};
    uint64_t dead_relay_number{0};
    uint64_t quarter_holder_number{0};
    uint64_t successor_number{0};
    std::vector<CBigNum> encrypted_secrets;
    Signature signature;

    SecretRecoveryMessage() { }

    static std::string Type() { return "secret_recovery"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(obituary_hash);
        READWRITE(dead_relay_number);
        READWRITE(quarter_holder_number);
        READWRITE(successor_number);
        READWRITE(encrypted_secrets);
        READWRITE(signature);
    )

    JSON(obituary_hash, dead_relay_number, quarter_holder_number, successor_number, encrypted_secrets, signature);

    DEPENDENCIES(obituary_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    Relay *GetKeyQuarterHolder(Data data);

    Relay *GetDeadRelay(Data data);

    Relay *GetSuccessor(Data data);
};


#endif //TELEPORT_SECRETRECOVERYMESSAGE_H
