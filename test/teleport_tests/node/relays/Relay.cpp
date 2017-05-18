#include <src/crypto/secp256k1point.h>
#include <src/crypto/bignum_hashes.h>
#include <src/base/util_hex.h>
#include <test/teleport_tests/node/relays/SuccessionCompletedMessage.h>
#include "Relay.h"
#include "test/teleport_tests/node/relays/RelayState.h"


#include "log.h"
#define LOG_CATEGORY "Relay.cpp"

using std::vector;

RelayJoinMessage Relay::GenerateJoinMessage(MemoryDataStore &keydata, uint160 mined_credit_message_hash)
{
    RelayJoinMessage join_message;
    join_message.mined_credit_message_hash = mined_credit_message_hash;
    join_message.PopulatePublicKeySet(keydata);
    return join_message;
}

KeyDistributionMessage Relay::GenerateKeyDistributionMessage(Data data, uint160 encoding_message_hash,
                                                             RelayState &relay_state)
{
    return GenerateKeyDistributionMessage(data, encoding_message_hash, relay_state, ALL_POSITIONS);
}

KeyDistributionMessage Relay::GenerateKeyDistributionMessage(Data data, uint160 encoding_message_hash,
                                                             RelayState &relay_state, uint32_t position)
{
    if (NumberOfKeyQuarterHoldersAssigned() == 0)
        relay_state.AssignKeyQuarterHoldersToRelay(*this);

    KeyDistributionMessage key_distribution_message;
    key_distribution_message.hash_of_message_containing_join = encoding_message_hash;
    key_distribution_message.relay_join_hash = hashes.join_message_hash;
    key_distribution_message.relay_number = number;
    key_distribution_message.position = (uint8_t) position;
    if (position == ALL_POSITIONS)
        key_distribution_message.PopulateEncryptedKeyQuarters(data.keydata, *this, relay_state);
    else
        key_distribution_message.PopulateEncryptedKeyQuarter(data.keydata, *this, relay_state, position);
    key_distribution_message.Sign(data);
    return key_distribution_message;
}

vector<CBigNum> Relay::PrivateKeyTwentyFourths(MemoryDataStore &keydata)
{
    vector<CBigNum> private_key_twentyfourths;

    for (auto public_key_twentyfourth : PublicKeyTwentyFourths())
    {
        CBigNum private_key_twentyfourth = keydata[public_key_twentyfourth]["privkey"];
        private_key_twentyfourths.push_back(private_key_twentyfourth);
    }
    return private_key_twentyfourths;
}

GoodbyeMessage Relay::GenerateGoodbyeMessage(Data data)
{
    GoodbyeMessage goodbye_message;
    goodbye_message.dead_relay_number = number;
    goodbye_message.Sign(data);
    return goodbye_message;
}

Point Relay::GenerateRecipientPublicKey(Point point_corresponding_to_secret)
{
    uint64_t start = GetRealTimeMicros();
    auto result = public_key_set.GenerateReceivingPublicKey(point_corresponding_to_secret);
    return result;
}

Point Relay::GenerateRecipientPublicKeyQuarter(Point point_corresponding_to_secret,
                                               uint8_t key_quarter_position)
{
    return public_key_set.GenerateReceivingPublicKeyQuarter(point_corresponding_to_secret, key_quarter_position);
}

CBigNum Relay::GenerateRecipientPrivateKey(Point point_corresponding_to_secret, Data data)
{
    return public_key_set.GenerateReceivingPrivateKey(point_corresponding_to_secret, data.keydata);
}

CBigNum Relay::GenerateRecipientPrivateKeyQuarter(Point point_corresponding_to_secret,
                                                  uint8_t key_quarter_position, Data data)
{
    return public_key_set.GenerateReceivingPrivateKeyQuarter(point_corresponding_to_secret,
                                                             key_quarter_position, data.keydata);
}

uint256 Relay::EncryptSecret(CBigNum secret)
{
    Point point_corresponding_to_secret = Point(secret);
    Point recipient_public_key = GenerateRecipientPublicKey(point_corresponding_to_secret);

    CBigNum shared_secret = Hash(secret * recipient_public_key);

    return (secret ^ shared_secret).getuint256();
}

CBigNum Relay::DecryptSecret(uint256 encrypted_secret, Point point_corresponding_to_secret, Data data)
{
    CBigNum recipient_private_key = GenerateRecipientPrivateKey(point_corresponding_to_secret, data);
    CBigNum shared_secret = Hash(recipient_private_key * point_corresponding_to_secret);
    CBigNum recovered_secret = CBigNum(encrypted_secret) ^ shared_secret;

    return Point(recovered_secret) == point_corresponding_to_secret ? recovered_secret : 0;
}

std::string PadWithZeroToEvenLength(std::string in)
{
    if (in.size() % 2 == 1)
        in = "0" + in;
    return in;
}

std::string PadWithZeroToLength64(std::string in)
{
    while (in.size() < 64)
        in = "0" + in;
    return in;
}

CBigNum StorePointInBigNum(Point point)
{
    CBigNum n;
    n.SetHex(HexStr(point.getvch()));
    return n;
}

Point RetrievePointFromBigNum(CBigNum n)
{
    if (n == 0)
        return Point(n);
    Point point;
    point.setvch(ParseHex(PadWithZeroToEvenLength(n.GetHex())));
    return point;
}

uint256 Relay::EncryptSecretPoint(Point secret_point)
{
    CBigNum encoded_point = StorePointInBigNum(secret_point);
    Point point_corresponding_to_secret = Point(encoded_point);
    Point recipient_public_key = GenerateRecipientPublicKey(point_corresponding_to_secret);

    CBigNum shared_secret = Hash(encoded_point * recipient_public_key);

    return (encoded_point ^ shared_secret).getuint256();
}

Point DecryptPointUsingHexPrefixes(CBigNum decrypted_secret, Point point_corresponding_to_secret)
{
    std::string secret_hex = decrypted_secret.GetHex();
    vector<std::string> prefixes{"0102", "0103", "0105"};
    Point recovered_point;
    for (auto prefix : prefixes)
    {
        recovered_point.setvch(ParseHex(prefix + PadWithZeroToLength64(secret_hex)));
        if (Point(StorePointInBigNum(recovered_point)) == point_corresponding_to_secret)
            return recovered_point;
    }
    return Point(CBigNum(0));
}

Point Relay::DecryptSecretPoint(uint256 encrypted_secret, Point point_corresponding_to_secret, Data data)
{
    CBigNum recipient_private_key = GenerateRecipientPrivateKey(point_corresponding_to_secret, data);
    CBigNum shared_secret = Hash(recipient_private_key * point_corresponding_to_secret);
    CBigNum recovered_secret = CBigNum(encrypted_secret) ^ shared_secret;

    return DecryptPointUsingHexPrefixes(recovered_secret, point_corresponding_to_secret);
}

vector<Point> Relay::PublicKeyTwentyFourths()
{
    return public_key_set.PublicKeyTwentyFourths();
}

uint64_t Relay::SuccessorNumberFromLatestSuccessionAttempt(Data data)
{
    uint64_t successor_number = 0;

    for (auto &successor_number_and_attempt : succession_attempts)
        successor_number = successor_number_and_attempt.first;

    return successor_number;
}

vector<uint64_t> Relay::KeyQuarterSharers(RelayState *relay_state)
{
    vector<uint64_t> sharers;

    for (auto &relay : relay_state->relays)
        if (ArrayContainsEntry(relay.key_quarter_holders, number))
            sharers.push_back(relay.number);

    return sharers;
}

vector<Relay *> Relay::QuarterHolders(Data data)
{
    vector<Relay *> quarter_holders;
    for (auto quarter_holder_number : key_quarter_holders)
        quarter_holders.push_back(data.relay_state->GetRelayByNumber(quarter_holder_number));

    return quarter_holders;
}

SuccessionCompletedMessage Relay::GenerateSuccessionCompletedMessage(SecretRecoveryMessage &recovery_message, Data data)
{
    auto dead_relay = recovery_message.GetDeadRelay(data);
    auto &attempt = dead_relay->succession_attempts[recovery_message.successor_number];
    SuccessionCompletedMessage succession_completed_message;
    succession_completed_message.recovery_message_hashes = attempt.recovery_message_hashes;
    succession_completed_message.dead_relay_number = dead_relay->number;
    succession_completed_message.successor_number = recovery_message.successor_number;
    succession_completed_message.Sign(data);
    return succession_completed_message;
}

KeyDistributionAuditMessage
Relay::GenerateKeyDistributionAuditMessage(uint160 hash_of_message_containing_relay_state, Data data)
{
    KeyDistributionAuditMessage audit_message;
    audit_message.Populate(this, hash_of_message_containing_relay_state, data);
    audit_message.Sign(data);
    return audit_message;
}

bool Relay::PrivateKeyIsPresent(Data data)
{
    return data.keydata[public_signing_key].HasProperty("privkey");
}

bool Relay::HasFourKeyQuarterHolders()
{
    return NumberOfKeyQuarterHoldersAssigned() == 4;
}

uint32_t Relay::NumberOfKeyQuarterHoldersAssigned()
{
    uint32_t number_assigned = 0;
    for (auto quarter_holder_number : key_quarter_holders)
        if (quarter_holder_number != 0)
            number_assigned += 1;
    return number_assigned;
}

void Relay::AddKeyQuarterHolder(uint64_t quarter_holder_number)
{
    for (uint32_t position = 0; position < 4; position++)
        if (key_quarter_holders[position] == 0)
        {
            key_quarter_holders[position] = quarter_holder_number;
            return;
        }
    log_ << "no empty spots for addition of quarter holder " << quarter_holder_number << "\n";
}

EncryptedKeyQuarter Relay::GetEncryptedKeyQuarter(uint32_t position, Data data)
{
    auto key_quarter_location = key_quarter_locations[position];
    std::string message_type = data.MessageType(key_quarter_location.message_hash);

    if (message_type == "key_distribution")
    {
        KeyDistributionMessage key_distribution_message = data.GetMessage(key_quarter_location.message_hash);
        return key_distribution_message.encrypted_key_quarters[key_quarter_location.position];
    }
    return EncryptedKeyQuarter();
}

void Relay::RecordSecretRecoveryMessage(SecretRecoveryMessage &secret_recovery_message)
{
    if (not succession_attempts.count(secret_recovery_message.successor_number))
        succession_attempts[secret_recovery_message.successor_number] = SuccessionAttempt();

    auto &attempt = succession_attempts[secret_recovery_message.successor_number];
    auto position = secret_recovery_message.position_of_quarter_holder;

    attempt.recovery_message_hashes[position] = secret_recovery_message.GetHash160();
}

void Relay::RecordSecretRecoveryComplaintAndRejectBadRecoveryMessage(SecretRecoveryComplaint &complaint)
{
    auto &attempt = succession_attempts[complaint.successor_number];
    attempt.recovery_complaint_hashes.push_back(complaint.GetHash160());
    attempt.rejected_recovery_messages.push_back(complaint.secret_recovery_message_hash);

    auto position = complaint.position_of_quarter_holder;
    attempt.recovery_message_hashes[position] = 0;
}

void Relay::RecordSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message)
{
    auto &attempt = succession_attempts[succession_completed_message.successor_number];
    attempt.succession_completed_message_hash = succession_completed_message.GetHash160();
}

void Relay::RecordSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message)
{
    auto &attempt = succession_attempts[failure_message.successor_number];
    RecoveryFailureAudit audit;
    attempt.audits[failure_message.GetHash160()] = audit;
}

void Relay::RecordRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message)
{
    auto &attempt = succession_attempts[audit_message.successor_number];
    auto &audit = attempt.audits[audit_message.failure_message_hash];
    audit.audit_message_hashes[audit_message.position_of_quarter_holder] = audit_message.GetHash160();
}

void Relay::RecordGoodbyeMessage(GoodbyeMessage &goodbye_message)
{
    hashes.goodbye_message_hash = goodbye_message.GetHash160();
}

void Relay::RecordSuccessionCompletedAuditMessage(SuccessionCompletedAuditMessage &audit_message)
{
    auto &attempt = succession_attempts[audit_message.successor_number];
    attempt.succession_completed_audit_message_hash = audit_message.GetHash160();
}
