#ifndef TELEPORT_RELAY_H
#define TELEPORT_RELAY_H


#include <src/crypto/point.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include <test/teleport_tests/node/relays/KeyDistributionAuditMessage.h>
#include <test/teleport_tests/node/relays/Task.h>
#include "test/teleport_tests/node/relays/RelayJoinMessage.h"
#include "test/teleport_tests/node/relays/KeyDistributionMessage.h"
#include "test/teleport_tests/node/relays/GoodbyeMessage.h"
#include "test/teleport_tests/node/relays/RelayMessageHashData.h"
#include "SuccessionCompletedMessage.h"
#include "KeyQuarterLocation.h"
#include "SecretRecoveryMessage.h"
#include "EncryptedKeyQuarter.h"
#include "SuccessionAttempt.h"
#include "SecretRecoveryComplaint.h"
#include "SecretRecoveryFailureMessage.h"
#include "RecoveryFailureAuditMessage.h"
#include "SuccessionCompletedAuditMessage.h"

#define ALIVE 0
#define SAID_GOODBYE 1
#define NOT_RESPONDING 2
#define MISBEHAVED 3


class Relay
{
public:
    uint64_t number{0};
    uint64_t current_successor_number{0};
    std::vector<uint64_t> previous_successors;

    RelayPublicKeySet public_key_set;

    uint8_t status{ALIVE};

    bool key_distribution_message_audited{false};
    bool join_message_was_encoded{false};

    std::array<uint64_t, 4> key_quarter_holders{ {0,0,0,0} };
    std::array<KeyQuarterLocation, 4> key_quarter_locations;
    std::vector<uint160> previous_key_quarter_locations;

    std::map<uint64_t, SuccessionAttempt> succession_attempts;
    RelayMessageHashData hashes;

    Point public_signing_key{SECP256K1, 0};

    uint32_t number_of_complaints_sent{0};
    std::vector<Task> tasks;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(number);
        READWRITE(current_successor_number);
        READWRITE(previous_successors);
        READWRITE(hashes);
        READWRITE(key_quarter_holders);
        READWRITE(key_quarter_locations);
        READWRITE(previous_key_quarter_locations);
        READWRITE(succession_attempts);
        READWRITE(public_key_set);
        READWRITE(public_signing_key);
        READWRITE(key_distribution_message_audited);
        READWRITE(join_message_was_encoded);
        READWRITE(number_of_complaints_sent);
        READWRITE(tasks);
    );

    JSON(number, current_successor_number, previous_successors,hashes, key_quarter_holders, key_quarter_locations,
         previous_key_quarter_locations, public_signing_key, key_distribution_message_audited,
         join_message_was_encoded, public_key_set, number_of_complaints_sent, tasks);

    HASH160();

    bool operator==(const Relay &other) const
    {
        return this->hashes.join_message_hash == other.hashes.join_message_hash;
    }

    bool HasFourKeyQuarterHolders();

    uint32_t NumberOfKeyQuarterHoldersAssigned();

    bool PrivateKeyIsPresent(Data data);

    std::vector<Point> PublicKeyTwentyFourths();

    std::vector<CBigNum> PrivateKeyTwentyFourths(MemoryDataStore &keydata);

    EncryptedKeyQuarter GetEncryptedKeyQuarter(uint32_t position, Data data);

    RelayJoinMessage GenerateJoinMessage(MemoryDataStore &keydata, uint160 mined_credit_message_hash);

    std::vector<uint64_t> KeyQuarterSharers(RelayState *relay_state);

    std::vector<Relay *> QuarterHolders(Data data);

    KeyDistributionMessage GenerateKeyDistributionMessage(Data databases, uint160 encoding_message_hash,
                                                          RelayState &state);

    GoodbyeMessage GenerateGoodbyeMessage(Data data);

    Point GenerateRecipientPublicKey(Point point_corresponding_to_secret);

    CBigNum GenerateRecipientPrivateKey(Point point_corresponding_to_secret, Data data);

    uint256 EncryptSecret(CBigNum secret);

    CBigNum DecryptSecret(uint256 encrypted_secret, Point point_corresponding_to_secret, Data data);

    uint64_t SuccessorNumberFromLatestSuccessionAttempt(Data data);

    CBigNum GenerateRecipientPrivateKeyQuarter(Point point_corresponding_to_secret,
                                               uint8_t key_quarter_position, Data data);

    Point GenerateRecipientPublicKeyQuarter(Point point_corresponding_to_secret, uint8_t key_quarter_position);

    uint256 EncryptSecretPoint(Point secret_point);

    Point DecryptSecretPoint(uint256 encrypted_secret, Point point_corresponding_to_secret, Data data);

    SuccessionCompletedMessage GenerateSuccessionCompletedMessage(SecretRecoveryMessage &recovery_message, Data data);

    KeyDistributionAuditMessage
    GenerateKeyDistributionAuditMessage(uint160 hash_of_message_containing_relay_state, Data data);

    void AddKeyQuarterHolder(uint64_t quarter_holder_number);

    void RecordSecretRecoveryMessage(SecretRecoveryMessage &secret_recovery_message);

    void RecordSecretRecoveryComplaintAndRejectBadRecoveryMessage(SecretRecoveryComplaint &complaint);

    void RecordSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message);

    void RecordSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message);

    void RecordRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message);

    void RecordGoodbyeMessage(GoodbyeMessage &goodbye_message);

    void RecordSuccessionCompletedAuditMessage(SuccessionCompletedAuditMessage &audit_message);

    KeyDistributionMessage
    GenerateKeyDistributionMessage(Data data, uint160 encoding_message_hash, RelayState &relay_state, uint32_t position);
};

Point DecryptPointUsingHexPrefixes(CBigNum decrypted_secret, Point point_corresponding_to_secret);

inline uint160 Death(uint64_t relay_number) { return relay_number; }
inline uint64_t GetDeadRelayNumber(uint160 death) { return death.GetLow64(); }
inline bool IsDeath(uint160 message) { return message == message.GetLow64(); }

#endif //TELEPORT_RELAY_H
