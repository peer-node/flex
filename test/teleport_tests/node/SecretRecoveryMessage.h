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
    std::vector<std::vector<CBigNum> > quartets_of_encrypted_shared_secret_quarters; // for the successor
    std::vector<std::vector<Point> > quartets_of_points_corresponding_to_shared_secret_quarters;
    std::vector<uint64_t> key_quarter_sharers;
    std::vector<uint8_t> key_quarter_positions;
    Signature signature;

    SecretRecoveryMessage() { }

    static std::string Type() { return "secret_recovery"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(obituary_hash);
        READWRITE(dead_relay_number);
        READWRITE(quarter_holder_number);
        READWRITE(successor_number);
        READWRITE(quartets_of_encrypted_shared_secret_quarters);
        READWRITE(quartets_of_points_corresponding_to_shared_secret_quarters);
        READWRITE(key_quarter_sharers);
        READWRITE(key_quarter_positions);
        READWRITE(signature);
    )

    JSON(obituary_hash, dead_relay_number, quarter_holder_number, successor_number,
         quartets_of_encrypted_shared_secret_quarters,
         quartets_of_points_corresponding_to_shared_secret_quarters,
         key_quarter_sharers, key_quarter_positions, signature);

    DEPENDENCIES(obituary_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    Relay *GetKeyQuarterHolder(Data data);

    Relay *GetDeadRelay(Data data);

    Relay *GetSuccessor(Data data);

    void PopulateSecrets(Data data);

    void PopulateKeyQuarterSharersAndPositions(Data data);

    void PopulateEncryptedSharedSecretQuarter(Relay &relay, Data data);

    void AddEncryptedSharedSecretQuarterForFourKeySixteenths(Relay &relay, uint8_t position, Data data);

    void PopulateEncryptedSharedSecretQuarterForKeySixteenth(
            Relay &key_sharer, uint8_t key_quarter_position, Data &data,
            std::vector<CBigNum> &encrypted_shared_secret_quarters_for_relay_key_parts,
            std::vector<Point> &points_corresponding_to_shared_secret_quarters,
            Relay *successor, int32_t key_sixteenth_position);

    std::vector<std::vector<Point> > RecoverSharedSecretQuartersForRelayKeyParts(Data data);

    std::vector<Point> RecoverQuartetOfSharedSecretQuarters(std::vector<CBigNum> encrypted_shared_secret_quarters,
                                                            std::vector<Point>, Data data);

    Point RecoverSharedSecretQuarter(CBigNum encrypted_shared_secret_quarter,
                                     Point point_corresponding_to_shared_secret_quarter, Data data);
};


#endif //TELEPORT_SECRETRECOVERYMESSAGE_H
