#include <src/base/util_hex.h>
#include "SecretRecoveryMessage.h"
#include "RelayState.h"

#include "log.h"

#define LOG_CATEGORY "SecretRecoveryMessage.cpp"

using std::vector;

Point SecretRecoveryMessage::VerificationKey(Data data)
{
    Relay *relay = GetKeyQuarterHolder(data);
    if (relay == NULL)
        return Point(SECP256K1, 0);
    return relay->public_signing_key;
}

Relay *SecretRecoveryMessage::GetKeyQuarterHolder(Data data)
{
    return data.relay_state->GetRelayByNumber(quarter_holder_number);
}

Relay *SecretRecoveryMessage::GetDeadRelay(Data data)
{
    return data.relay_state->GetRelayByNumber(dead_relay_number);
}

Relay *SecretRecoveryMessage::GetSuccessor(Data data)
{
    return data.relay_state->GetRelayByNumber(successor_number);
}

void SecretRecoveryMessage::PopulateSecrets(Data data)
{
    for (auto &relay : data.relay_state->relays)
    {
        if (VectorContainsEntry(relay.holders.key_quarter_holders, dead_relay_number) and
                relay.hashes.key_distribution_message_hash != 0)
            PopulateEncryptedSharedSecretQuarter(relay, data);
    }
}

void SecretRecoveryMessage::PopulateEncryptedSharedSecretQuarter(Relay &relay, Data data)
{
    uint8_t position = (uint8_t)PositionOfEntryInVector(dead_relay_number, relay.holders.key_quarter_holders);
    key_quarter_sharers.push_back(relay.number);
    key_quarter_positions.push_back(position);

    AddEncryptedSharedSecretQuarterForFourKeySixteenths(relay, position, data);
}

void SecretRecoveryMessage::AddEncryptedSharedSecretQuarterForFourKeySixteenths(Relay &key_sharer,
                                                                                uint8_t key_quarter_position, Data data)
{
    vector<uint256> encrypted_shared_secret_quarters;
    vector<Point> points_corresponding_to_shared_secret_quarters;

    auto successor = GetSuccessor(data);

    for (int32_t key_sixteenth_position = 4 * key_quarter_position;
         key_sixteenth_position < 4 * (key_quarter_position + 1);
         key_sixteenth_position++)
    {
        PopulateEncryptedSharedSecretQuarterForKeySixteenth(key_sharer,
                                                            encrypted_shared_secret_quarters,
                                                            points_corresponding_to_shared_secret_quarters, successor,
                                                            key_sixteenth_position, data);
    }
    quartets_of_encrypted_shared_secret_quarters.push_back(encrypted_shared_secret_quarters);
    quartets_of_points_corresponding_to_shared_secret_quarters.push_back(points_corresponding_to_shared_secret_quarters);
}

void SecretRecoveryMessage::PopulateEncryptedSharedSecretQuarterForKeySixteenth(
        Relay &key_sharer,
        vector<uint256> &encrypted_shared_secret_quarters_for_relay_key_parts,
        vector<Point> &points_corresponding_to_shared_secret_quarters,
        Relay *successor,
        int32_t key_sixteenth_position,
        Data &data)
{
    Point public_key_sixteenth = key_sharer.PublicKeySixteenths()[key_sixteenth_position];

    auto dead_relay = GetDeadRelay(data);

    uint8_t quarter_holder_position = (uint8_t)PositionOfEntryInVector(quarter_holder_number,
                                                                       dead_relay->holders.key_quarter_holders);

    CBigNum recipient_key_quarter = dead_relay->GenerateRecipientPrivateKeyQuarter(public_key_sixteenth,
                                                                                   quarter_holder_position,
                                                                                   data);
    Point shared_secret_quarter = recipient_key_quarter * public_key_sixteenth;

    uint256 encrypted_shared_secret_quarter = successor->EncryptSecretPoint(shared_secret_quarter);

    points_corresponding_to_shared_secret_quarters.push_back(Point(StorePointInBigNum(shared_secret_quarter)));
    encrypted_shared_secret_quarters_for_relay_key_parts.push_back(encrypted_shared_secret_quarter);
}

vector<vector<Point> > SecretRecoveryMessage::RecoverSharedSecretQuartersForRelayKeyParts(Data data)
{
    vector<vector<Point> > shared_secret_quarters;

    for (uint32_t i = 0; i < quartets_of_encrypted_shared_secret_quarters.size(); i++)
    {
        auto quartet = quartets_of_encrypted_shared_secret_quarters[i];
        auto quartet_points = quartets_of_points_corresponding_to_shared_secret_quarters[i];

        if (quartet.size() != 4 or quartet_points.size() != 4)
        {
            return shared_secret_quarters;
        }

        shared_secret_quarters.push_back(RecoverQuartetOfSharedSecretQuarters(quartet, quartet_points, data));
    }
    return shared_secret_quarters;
}

vector<Point> SecretRecoveryMessage::RecoverQuartetOfSharedSecretQuarters(
        vector<uint256> encrypted_shared_secret_quarters,
        vector<Point> points_corresponding_to_shared_secret_quarters, Data data)
{
    vector<Point> shared_secret_quarters;

    for (uint32_t i = 0; i < 4; i++)
    {
        Point shared_secret_quarter = RecoverSharedSecretQuarter(encrypted_shared_secret_quarters[i],
                                                                 points_corresponding_to_shared_secret_quarters[i],
                                                                 data);
        shared_secret_quarters.push_back(shared_secret_quarter);
    }

    return shared_secret_quarters;
}

Point SecretRecoveryMessage::RecoverSharedSecretQuarter(uint256 encrypted_shared_secret_quarter,
                                                        Point point_corresponding_to_shared_secret_quarter, Data data)
{
    auto successor = GetSuccessor(data);
    return successor->DecryptSecretPoint(encrypted_shared_secret_quarter,
                                         point_corresponding_to_shared_secret_quarter, data);
}

bool SomePointsAreAtInfinity(vector<vector<Point> > array_of_points)
{
    for (auto &row : array_of_points)
        for (auto &point : row)
            if (point.IsAtInfinity())
                return true;
    return false;
}

void AddArrayOfPointsToSum(vector<vector<Point> > array_of_points, vector<vector<Point> > &sum)
{
    if (sum.size() == 0)
        sum.resize(array_of_points.size());

    for (int row = 0; row < array_of_points.size(); row++)
    {
        for (int column = 0; column < array_of_points[row].size(); column++)
        {
            if (sum[row].size() <= column)
                sum[row].push_back(Point(CBigNum(0)));
            sum[row][column] += array_of_points[row][column];
        }
    }
}

bool SecretRecoveryMessage::RecoverSecretsFromSecretRecoveryMessages(vector<uint160> recovery_message_hashes,
                                                                     uint32_t &failure_sharer_position,
                                                                     uint32_t &failure_key_part_position,
                                                                     Data data)
{
    auto array_of_shared_secrets = GetSharedSecrets(recovery_message_hashes, data);

    return RecoverSecretsFromArrayOfSharedSecrets(array_of_shared_secrets, recovery_message_hashes,
                                                  failure_sharer_position, failure_key_part_position, data);
}

vector<vector<Point> > SecretRecoveryMessage::GetSharedSecrets(vector<uint160> recovery_message_hashes, Data data)
{
    vector<vector<Point> > array_of_shared_secrets;

    for (auto recovery_message_hash : recovery_message_hashes)
    {
        SecretRecoveryMessage secret_recovery_message = data.GetMessage(recovery_message_hash);
        auto shared_secret_quarters = secret_recovery_message.RecoverSharedSecretQuartersForRelayKeyParts(data);

        if (SomePointsAreAtInfinity(shared_secret_quarters))
        {
            return array_of_shared_secrets;
        }

        AddArrayOfPointsToSum(shared_secret_quarters, array_of_shared_secrets);
    }
    return array_of_shared_secrets;
}

bool SecretRecoveryMessage::RecoverSecretsFromArrayOfSharedSecrets(vector<vector<Point> > array_of_shared_secrets,
                                                                   vector<uint160> recovery_message_hashes,
                                                                   uint32_t &failure_sharer_position,
                                                                   uint32_t &failure_key_part_position, Data data)
{
    SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hashes[0]);

    for (uint32_t i = 0; i < recovery_message.key_quarter_sharers.size(); i++)
        if (not RecoverFourKeySixteenths(recovery_message.key_quarter_sharers[i],
                                         recovery_message.dead_relay_number,
                                         recovery_message.key_quarter_positions[i],
                                         array_of_shared_secrets[i],
                                         failure_key_part_position,
                                         data))
        {
            failure_sharer_position = i;
            return false;
        }
    return true;
}

bool SecretRecoveryMessage::RecoverFourKeySixteenths(uint64_t key_sharer_number, uint64_t dead_relay_number,
                                                     uint8_t key_quarter_position, vector<Point> shared_secrets,
                                                     uint32_t &failure_key_part_position, Data data)
{
    auto key_sharer = data.relay_state->GetRelayByNumber(key_sharer_number);
    auto dead_relay = data.relay_state->GetRelayByNumber(dead_relay_number);

    vector<uint256> encrypted_key_sixteenths = GetEncryptedKeySixteenthsSentFromSharerToRelay(key_sharer,
                                                                                              dead_relay, data);

    for (uint32_t i = 0; i < 4; i++)
    {
        uint32_t key_sixteenth_position = 4 * (uint32_t)key_quarter_position + i;

        Point public_key_sixteenth = key_sharer->PublicKeySixteenths()[key_sixteenth_position];
        CBigNum decrypted_key_sixteenth = CBigNum(encrypted_key_sixteenths[i]) ^ Hash(shared_secrets[i]);

        if (Point(decrypted_key_sixteenth) != public_key_sixteenth)
        {
            failure_key_part_position = i;
            return false;
        }
        data.keydata[public_key_sixteenth]["privkey"] = decrypted_key_sixteenth;
    }
    return true;
}

vector<uint256> SecretRecoveryMessage::GetEncryptedKeySixteenthsSentFromSharerToRelay(Relay *sharer, Relay *relay,
                                                                                      Data data)
{
    KeyDistributionMessage key_distribution_message = data.GetMessage(sharer->hashes.key_distribution_message_hash);
    auto &encrypted_secrets = key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders;

    int64_t key_quarter_position = PositionOfEntryInVector(relay->number, sharer->holders.key_quarter_holders);

    vector<uint256> encrypted_key_sixteenths;
    for (int64_t key_sixteenth_position = 4 * key_quarter_position;
            key_sixteenth_position < 4 + 4 * key_quarter_position; key_sixteenth_position++)
        encrypted_key_sixteenths.push_back(encrypted_secrets[key_sixteenth_position]);
    return encrypted_key_sixteenths;
}

bool SecretRecoveryMessage::ValidateSizes()
{
    uint64_t number_of_key_sharers = key_quarter_sharers.size();

    if (key_quarter_positions.size() != number_of_key_sharers or
            quartets_of_encrypted_shared_secret_quarters.size() != number_of_key_sharers or
            quartets_of_points_corresponding_to_shared_secret_quarters.size() != number_of_key_sharers)
        return false;
    for (auto &quartet : quartets_of_encrypted_shared_secret_quarters)
        if (quartet.size() != 4)
            return false;
    for (auto &quartet : quartets_of_points_corresponding_to_shared_secret_quarters)
        if (quartet.size() != 4)
            return false;

    return true;
}

bool SecretRecoveryMessage::IsValid(Data data)
{
    if (not ValidateSizes())
        return false;

    if (DurationWithoutResponseFromQuarterHolderHasElapsedSinceObituary(data))
        return false;

    Obituary obituary = data.GetMessage(obituary_hash);
    auto dead_relay = data.relay_state->GetRelayByNumber(obituary.dead_relay_number);

    return DeadRelayAndSuccessorNumberMatchObituary(obituary, data) and
           ReferencedQuarterHoldersAreValid(dead_relay, data) and
           AllTheDeadRelaysKeySharersAreIncluded(dead_relay, data) and
           KeyQuarterPositionsAreAllLessThanFour();
}

bool SecretRecoveryMessage::DurationWithoutResponseFromQuarterHolderHasElapsedSinceObituary(Data data)
{
    auto quarter_holder = data.relay_state->GetRelayByNumber(quarter_holder_number);
    return quarter_holder == NULL or quarter_holder->hashes.obituary_hash != 0;
}

bool SecretRecoveryMessage::DeadRelayAndSuccessorNumberMatchObituary(Obituary &obituary, Data data)
{
    return obituary.dead_relay_number == dead_relay_number and obituary.successor_number == successor_number;
}

bool SecretRecoveryMessage::KeyQuarterPositionsAreAllLessThanFour()
{
    for (auto &position : key_quarter_positions)
        if (position >= 4)
            return false;
    return true;
}

bool SecretRecoveryMessage::AllTheDeadRelaysKeySharersAreIncluded(Relay *dead_relay, Data data)
{
    uint32_t number_of_sharers = 0;
    for (auto &relay : data.relay_state->relays)
        if (VectorContainsEntry(relay.holders.key_quarter_holders, dead_relay->number))
            number_of_sharers++;
    return number_of_sharers == key_quarter_sharers.size();
}

bool SecretRecoveryMessage::ReferencedQuarterHoldersAreValid(Relay *dead_relay, Data data)
{
    if (not VectorContainsEntry(dead_relay->holders.key_quarter_holders, quarter_holder_number))
        return false;

    for (auto &sharer_number : key_quarter_sharers)
    {
        auto sharer = data.relay_state->GetRelayByNumber(sharer_number);
        if (sharer == NULL or not VectorContainsEntry(sharer->holders.key_quarter_holders, dead_relay->number))
            return false;
    }
    return true;
}
