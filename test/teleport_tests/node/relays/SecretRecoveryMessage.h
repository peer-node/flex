#ifndef TELEPORT_SECRETRECOVERYMESSAGE_H
#define TELEPORT_SECRETRECOVERYMESSAGE_H


#include <src/crypto/bignum.h>
#include <src/crypto/signature.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "RelayJoinMessage.h"
#include "test/teleport_tests/node/Data.h"

class Relay;

class SecretRecoveryMessage
{
public:
    uint64_t dead_relay_number{0};
    uint64_t quarter_holder_number{0};
    uint64_t successor_number{0};
    uint8_t position_of_quarter_holder{0};

    std::vector<std::vector<uint256> > sextets_of_encrypted_shared_secret_quarters; // for the successor
    std::vector<std::vector<Point> > sextets_of_points_corresponding_to_shared_secret_quarters;
    std::vector<uint64_t> key_quarter_sharers;
    std::vector<uint8_t> key_quarter_positions;

    uint160 replaced_message{0};
    uint160 reason_for_replacement{0};

    Signature signature;

    static std::string Type() { return "secret_recovery"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(dead_relay_number);
        READWRITE(quarter_holder_number);
        READWRITE(successor_number);
        READWRITE(position_of_quarter_holder);
        READWRITE(sextets_of_encrypted_shared_secret_quarters);
        READWRITE(sextets_of_points_corresponding_to_shared_secret_quarters);
        READWRITE(key_quarter_sharers);
        READWRITE(key_quarter_positions);
        READWRITE(replaced_message);
        READWRITE(reason_for_replacement);
        READWRITE(signature);
    )

    JSON(dead_relay_number, quarter_holder_number, successor_number,
         position_of_quarter_holder, sextets_of_encrypted_shared_secret_quarters,
         sextets_of_points_corresponding_to_shared_secret_quarters,
         key_quarter_sharers, key_quarter_positions, replaced_message, reason_for_replacement, signature);

    std::vector<uint160> Dependencies()
    {
        std::vector<uint160> dependencies;
        if (replaced_message != 0)
        {
            dependencies.push_back(replaced_message);
            dependencies.push_back(reason_for_replacement);
        }
        return dependencies;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    Relay *GetKeyQuarterHolder(Data data);

    Relay *GetDeadRelay(Data data);

    Relay *GetSuccessor(Data data);

    void PopulateSecrets(Data data);

    void PopulateEncryptedSharedSecretQuarter(Relay &relay, Data data);

    void AddEncryptedSharedSecretQuarterForFourKeyTwentyFourths(Relay &relay, uint8_t position, Data data);

    void PopulateEncryptedSharedSecretQuarterForKeyTwentyFourth(
            Relay &key_sharer, std::vector<uint256> &encrypted_shared_secret_quarters_for_relay_key_parts,
            std::vector<Point> &points_corresponding_to_shared_secret_quarters,
            Relay *successor, int32_t key_sixteenth_position, Data &data);

    std::vector<std::vector<Point> > RecoverSharedSecretQuartersForRelayKeyParts(Data data);

    std::vector<Point> RecoverSextetOfSharedSecretQuarters(std::vector<uint256> encrypted_shared_secret_quarters,
                                                            std::vector<Point>, Data data);

    Point RecoverSharedSecretQuarter(uint256 encrypted_shared_secret_quarter,
                                     Point point_corresponding_to_shared_secret_quarter, Data data);

    static std::vector<std::vector<Point> > GetSharedSecrets(std::array<uint160, 4> recovery_message_hashes, Data data);

    static bool RecoverSecretsFromArrayOfSharedSecrets(std::vector<std::vector<Point> > array_of_shared_secrets,
                                                       std::array<uint160, 4> recovery_message_hashes,
                                                       uint32_t &failure_sharer_position,
                                                       uint32_t &failure_key_part_position, Data data);

    static bool RecoverSixKeyTwentyFourths(uint64_t key_sharer_number, uint64_t dead_relay_number,
                                           uint8_t key_quarter_position, std::vector<Point> shared_secrets,
                                           uint32_t &failure_key_part_position, Data data);

    static bool RecoverSecretsFromSecretRecoveryMessages(std::array<uint160, 4> recovery_message_hashes,
                                                         uint32_t &failure_sharer_position,
                                                         uint32_t &failure_key_part_position, Data data);

    bool ValidateSizes();

    bool IsValid(Data data);

    bool ReferencedQuarterHoldersAreValid(Relay *dead_relay, Data data);

    bool KeyQuarterPositionsAreAllLessThanFour();

    bool AllTheDeadRelaysKeySharersAreIncluded(Relay *dead_relay, Data data);
};

std::string PadWithZeroToEvenLength(std::string in);

CBigNum StorePointInBigNum(Point point);

Point RetrievePointFromBigNum(CBigNum n);

#endif //TELEPORT_SECRETRECOVERYMESSAGE_H
