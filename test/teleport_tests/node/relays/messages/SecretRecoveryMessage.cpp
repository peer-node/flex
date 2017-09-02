#include <src/base/util_hex.h>
#include "SecretRecoveryMessage.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"

using std::vector;

#include "log.h"
#define LOG_CATEGORY "SecretRecoveryMessage.cpp"


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
        if (ArrayContainsEntry(relay.key_quarter_holders, dead_relay_number))
        {
            auto position = PositionOfEntryInArray(dead_relay_number, relay.key_quarter_holders);
            if (relay.key_quarter_locations[position].message_hash != 0)
                PopulateEncryptedSharedSecretQuarter(relay, data);
        }
    }
}

void SecretRecoveryMessage::PopulateEncryptedSharedSecretQuarter(Relay &relay, Data data)
{
    uint8_t position = (uint8_t)PositionOfEntryInArray(dead_relay_number, relay.key_quarter_holders);
    key_quarter_sharers.push_back(relay.number);
    key_quarter_positions.push_back(position);

    AddEncryptedSharedSecretQuartersForSixKeyTwentyFourths(relay, position, data);
}

void SecretRecoveryMessage::AddEncryptedSharedSecretQuartersForSixKeyTwentyFourths(Relay &key_sharer,
                                                                                   uint8_t key_quarter_position,
                                                                                   Data data)
{
    vector<uint256> encrypted_shared_secret_quarters;
    vector<Point> points_corresponding_to_shared_secret_quarters;

    auto successor = GetSuccessor(data);

    for (int32_t key_twentyfourth_position = 6 * key_quarter_position;
         key_twentyfourth_position < 6 * (key_quarter_position + 1);
         key_twentyfourth_position++)
    {
        PopulateEncryptedSharedSecretQuarterForKeyTwentyFourth(key_sharer,
                                                               encrypted_shared_secret_quarters,
                                                               points_corresponding_to_shared_secret_quarters, successor,
                                                               key_twentyfourth_position, data);
    }
    sextets_of_encrypted_shared_secret_quarters.push_back(encrypted_shared_secret_quarters);
    sextets_of_points_corresponding_to_shared_secret_quarters.push_back(points_corresponding_to_shared_secret_quarters);
}

void SecretRecoveryMessage::PopulateEncryptedSharedSecretQuarterForKeyTwentyFourth(
        Relay &key_sharer,
        vector<uint256> &encrypted_shared_secret_quarters_for_relay_key_parts,
        vector<Point> &points_corresponding_to_shared_secret_quarters,
        Relay *successor,
        int32_t key_twentyfourth_position,
        Data &data)
{
    Point public_key_twentyfourth = key_sharer.PublicKeyTwentyFourths()[key_twentyfourth_position];

    auto dead_relay = GetDeadRelay(data);

    uint8_t quarter_holder_position = (uint8_t)PositionOfEntryInArray(quarter_holder_number,
                                                                      dead_relay->key_quarter_holders);

    CBigNum recipient_key_quarter = dead_relay->GenerateRecipientPrivateKeyQuarter(public_key_twentyfourth,
                                                                                   quarter_holder_position,
                                                                                   data);
    Point shared_secret_quarter = recipient_key_quarter * public_key_twentyfourth;

    uint256 encrypted_shared_secret_quarter = successor->EncryptSecretPoint(shared_secret_quarter);

    points_corresponding_to_shared_secret_quarters.push_back(Point(StorePointInBigNum(shared_secret_quarter)));
    encrypted_shared_secret_quarters_for_relay_key_parts.push_back(encrypted_shared_secret_quarter);
}

vector<vector<Point> > SecretRecoveryMessage::RecoverSharedSecretQuartersForRelayKeyParts(Data data)
{
    vector<vector<Point> > shared_secret_quarters;

    for (uint32_t i = 0; i < sextets_of_encrypted_shared_secret_quarters.size(); i++)
    {
        auto sextet = sextets_of_encrypted_shared_secret_quarters[i];
        auto sextet_points = sextets_of_points_corresponding_to_shared_secret_quarters[i];

        if (sextet.size() != 6 or sextet_points.size() != 6)
        {
            return shared_secret_quarters;
        }

        shared_secret_quarters.push_back(RecoverSextetOfSharedSecretQuarters(sextet, sextet_points, data));
    }
    return shared_secret_quarters;
}

vector<Point> SecretRecoveryMessage::RecoverSextetOfSharedSecretQuarters(
        vector<uint256> encrypted_shared_secret_quarters,
        vector<Point> points_corresponding_to_shared_secret_quarters, Data data)
{
    vector<Point> shared_secret_quarters;

    for (uint32_t i = 0; i < 6; i++)
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

bool SecretRecoveryMessage::RecoverSecretsFromSecretRecoveryMessages(std::array<uint160, 4> recovery_message_hashes,
                                                                     uint32_t &failure_sharer_position,
                                                                     uint32_t &failure_key_part_position,
                                                                     Data data)
{
    auto array_of_shared_secrets = GetSharedSecrets(recovery_message_hashes, data);
    auto result = RecoverSecretsFromArrayOfSharedSecrets(array_of_shared_secrets, recovery_message_hashes,
                                                  failure_sharer_position, failure_key_part_position, data);
    return result;
}

vector<vector<Point> > SecretRecoveryMessage::GetSharedSecrets(std::array<uint160, 4> recovery_message_hashes, Data data)
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
                                                                   std::array<uint160, 4> recovery_message_hashes,
                                                                   uint32_t &failure_sharer_position,
                                                                   uint32_t &failure_key_part_position, Data data)
{
    SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hashes[0]);

    for (uint32_t i = 0; i < recovery_message.key_quarter_sharers.size(); i++)
        if (array_of_shared_secrets.size() <= i or
                not RecoverSixKeyTwentyFourths(recovery_message.key_quarter_sharers[i],
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

bool SecretRecoveryMessage::RecoverSixKeyTwentyFourths(uint64_t key_sharer_number, uint64_t dead_relay_number,
                                                       uint8_t key_quarter_position, vector<Point> shared_secrets,
                                                       uint32_t &failure_key_part_position, Data data)
{
    auto key_sharer = data.relay_state->GetRelayByNumber(key_sharer_number);
    auto dead_relay = data.relay_state->GetRelayByNumber(dead_relay_number);

    auto encrypted_key_quarter = key_sharer->GetEncryptedKeyQuarter(key_quarter_position, data);

    for (uint32_t i = 0; i < 6; i++)
    {
        uint32_t key_twentyfourth_position = 6 * (uint32_t)key_quarter_position + i;
        auto encrypted_key_twentyfourth = encrypted_key_quarter.encrypted_key_twentyfourths[i];

        Point public_key_twentyfourth = key_sharer->PublicKeyTwentyFourths()[key_twentyfourth_position];
        CBigNum decrypted_key_twentyfourth = CBigNum(encrypted_key_twentyfourth) ^ Hash(shared_secrets[i]);

        if (Point(decrypted_key_twentyfourth) != public_key_twentyfourth)
        {
            failure_key_part_position = i;
            return false;
        }
        data.keydata[public_key_twentyfourth]["privkey"] = decrypted_key_twentyfourth;
    }
    return true;
}

bool SecretRecoveryMessage::ValidateSizes()
{
    uint64_t number_of_key_sharers = key_quarter_sharers.size();

    if (key_quarter_positions.size() != number_of_key_sharers or
            sextets_of_encrypted_shared_secret_quarters.size() != number_of_key_sharers or
            sextets_of_points_corresponding_to_shared_secret_quarters.size() != number_of_key_sharers)
        return false;
    for (auto &sextet : sextets_of_encrypted_shared_secret_quarters)
        if (sextet.size() != 6)
            return false;
    for (auto &sextet : sextets_of_points_corresponding_to_shared_secret_quarters)
        if (sextet.size() != 6)
            return false;

    return true;
}

bool SecretRecoveryMessage::IsValid(Data data)
{
    if (not ValidateSizes())
        return false;
    auto dead_relay = data.relay_state->GetRelayByNumber(dead_relay_number);
    if (not ReferencedQuarterHoldersAreValid(dead_relay, data))
        return false;
    if (not AllTheDeadRelaysKeySharersAreIncluded(dead_relay, data))
        return false;
    if (not KeyQuarterPositionsAreAllLessThanFour())
        return false;;
    return true;
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
        if (ArrayContainsEntry(relay.key_quarter_holders, dead_relay->number))
        {
            auto position = PositionOfEntryInArray(dead_relay->number, relay.key_quarter_holders);
            if (relay.key_quarter_locations[position].message_hash != 0)
                number_of_sharers++;
        }
    return number_of_sharers == key_quarter_sharers.size();
}

bool SecretRecoveryMessage::ReferencedQuarterHoldersAreValid(Relay *dead_relay, Data data)
{
    if (not ArrayContainsEntry(dead_relay->key_quarter_holders, quarter_holder_number))
        return false;

    for (auto &sharer_number : key_quarter_sharers)
    {
        auto sharer = data.relay_state->GetRelayByNumber(sharer_number);
        if (sharer == NULL or not ArrayContainsEntry(sharer->key_quarter_holders, dead_relay->number))
            return false;
    }
    return true;
}
