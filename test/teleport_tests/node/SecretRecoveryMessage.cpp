#include <src/base/util_hex.h>
#include "SecretRecoveryMessage.h"
#include "RelayState.h"

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
    PopulateKeyQuarterSharersAndPositions(data);
}

void SecretRecoveryMessage::PopulateKeyQuarterSharersAndPositions(Data data)
{
    for (auto &relay : data.relay_state->relays)
    {
        if (VectorContainsEntry(relay.key_quarter_holders, dead_relay_number) and
                relay.key_distribution_message_hash != 0)
            PopulateEncryptedSharedSecretQuarter(relay, data);
    }
}

void SecretRecoveryMessage::PopulateEncryptedSharedSecretQuarter(Relay &relay, Data data)
{
    uint8_t position = (uint8_t)PositionOfEntryInVector(dead_relay_number, relay.key_quarter_holders);
    key_quarter_sharers.push_back(relay.number);
    key_quarter_positions.push_back(position);

    AddEncryptedSharedSecretQuarterForFourKeySixteenths(relay, position, data);
}

std::string PadWithZero(std::string in)
{
    if (in.size() % 2 == 1)
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
    Point point;
    point.setvch(ParseHex(PadWithZero(n.GetHex())));
    return point;
}

void SecretRecoveryMessage::AddEncryptedSharedSecretQuarterForFourKeySixteenths(Relay &key_sharer,
                                                                                uint8_t key_quarter_position, Data data)
{
    std::vector<CBigNum> encrypted_shared_secret_quarters;
    std::vector<Point> points_corresponding_to_shared_secret_quarters;

    auto successor = GetSuccessor(data);

    for (int32_t key_sixteenth_position = 4 * key_quarter_position;
         key_sixteenth_position < 4 * (key_quarter_position + 1);
         key_sixteenth_position++)
    {
        PopulateEncryptedSharedSecretQuarterForKeySixteenth(key_sharer, key_quarter_position, data,
                                                            encrypted_shared_secret_quarters,
                                                            points_corresponding_to_shared_secret_quarters, successor,
                                                            key_sixteenth_position);
    }
    quartets_of_encrypted_shared_secret_quarters.push_back(encrypted_shared_secret_quarters);
    quartets_of_points_corresponding_to_shared_secret_quarters.push_back(points_corresponding_to_shared_secret_quarters);
}

void SecretRecoveryMessage::PopulateEncryptedSharedSecretQuarterForKeySixteenth(
        Relay &key_sharer,
        uint8_t key_quarter_position,
        Data &data,
        std::vector<CBigNum> &encrypted_shared_secret_quarters_for_relay_key_parts,
        std::vector<Point> &points_corresponding_to_shared_secret_quarters,
        Relay *successor,
        int32_t key_sixteenth_position)
{
    Point public_key_sixteenth = key_sharer.PublicKeySixteenths()[key_sixteenth_position];

    auto dead_relay = GetDeadRelay(data);

    uint8_t quarter_holder_position = (uint8_t)PositionOfEntryInVector(quarter_holder_number,
                                                                       dead_relay->key_quarter_holders);

    CBigNum recipient_key_quarter = dead_relay->GenerateRecipientPrivateKeyQuarter(public_key_sixteenth,
                                                                                   quarter_holder_position,
                                                                                   data);
    Point shared_secret_quarter = recipient_key_quarter * public_key_sixteenth;
    CBigNum encoded_shared_secret_quarter = StorePointInBigNum(shared_secret_quarter);
    CBigNum encrypted_shared_secret_quarter = successor->EncryptSecret(encoded_shared_secret_quarter);

    points_corresponding_to_shared_secret_quarters.push_back(Point(encoded_shared_secret_quarter));
    encrypted_shared_secret_quarters_for_relay_key_parts.push_back(encrypted_shared_secret_quarter);
}

std::vector<std::vector<Point> > SecretRecoveryMessage::RecoverSharedSecretQuartersForRelayKeyParts(Data data)
{
    std::vector<std::vector<Point> > shared_secret_quarters;

    for (uint32_t i = 0; i < quartets_of_encrypted_shared_secret_quarters.size(); i++)
    {
        auto quartet = quartets_of_encrypted_shared_secret_quarters[i];
        auto quartet_points = quartets_of_points_corresponding_to_shared_secret_quarters[i];

        if (quartet.size() != 4 or quartet_points.size() != 4)
        {
            log_ << "aborting\n";
            return shared_secret_quarters;
        }

        shared_secret_quarters.push_back(RecoverQuartetOfSharedSecretQuarters(quartet, quartet_points, data));
    }

    return shared_secret_quarters;
}

std::vector<Point> SecretRecoveryMessage::RecoverQuartetOfSharedSecretQuarters(
        std::vector<CBigNum> encrypted_shared_secret_quarters,
        std::vector<Point> points_corresponding_to_shared_secret_quarters, Data data)
{
    std::vector<Point> shared_secret_quarters;

    for (uint32_t i = 0; i < 4; i++)
    {
        Point shared_secret_quarter = RecoverSharedSecretQuarter(encrypted_shared_secret_quarters[i],
                                                                 points_corresponding_to_shared_secret_quarters[i],
                                                                 data);
        shared_secret_quarters.push_back(shared_secret_quarter);
    }

    return shared_secret_quarters;
}

Point SecretRecoveryMessage::RecoverSharedSecretQuarter(CBigNum encrypted_shared_secret_quarter,
                                                        Point point_corresponding_to_shared_secret_quarter, Data data)
{
    auto successor = GetSuccessor(data);
    CBigNum encoded_shared_secret_quarter = successor->DecryptSecret(encrypted_shared_secret_quarter,
                                                                     point_corresponding_to_shared_secret_quarter,
                                                                     data);
    return RetrievePointFromBigNum(encoded_shared_secret_quarter);
}