#include <src/crypto/secp256k1point.h>
#include <src/crypto/bignum_hashes.h>
#include <src/base/util_hex.h>
#include "Relay.h"
#include "test/teleport_tests/node/relay_handler/RelayState.h"
#include "test/teleport_tests/node/relay_handler/RelayKeyHolderData.h"


#include "log.h"
#define LOG_CATEGORY "Relay.cpp"

using std::vector;

RelayJoinMessage Relay::GenerateJoinMessage(MemoryDataStore &keydata, uint160 mined_credit_message_hash)
{
    RelayJoinMessage join_message;
    join_message.mined_credit_message_hash = mined_credit_message_hash;
    join_message.PopulatePublicKeySet(keydata);
    join_message.PopulatePrivateKeySixteenths(keydata);
    return join_message;
}

KeyDistributionMessage Relay::GenerateKeyDistributionMessage(Data data, uint160 encoding_message_hash, RelayState &relay_state)
{
    if (holders.key_quarter_holders.size() == 0)
        relay_state.AssignKeyPartHoldersToRelay(*this, encoding_message_hash);

    KeyDistributionMessage key_distribution_message;
    key_distribution_message.encoding_message_hash = encoding_message_hash;
    key_distribution_message.relay_join_hash = hashes.join_message_hash;
    key_distribution_message.relay_number = number;
    key_distribution_message.PopulateKeySixteenthsEncryptedForKeyQuarterHolders(data.keydata, *this, relay_state);
    key_distribution_message.PopulateKeySixteenthsEncryptedForKeySixteenthHolders(data.keydata, *this, relay_state);
    key_distribution_message.Sign(data);
    return key_distribution_message;
}

std::vector<std::vector<uint64_t> > Relay::KeyPartHolderGroups()
{
    return std::vector<std::vector<uint64_t> >{holders.key_quarter_holders,
                                               holders.first_set_of_key_sixteenth_holders,
                                               holders.second_set_of_key_sixteenth_holders};
}

std::vector<CBigNum> Relay::PrivateKeySixteenths(MemoryDataStore &keydata)
{
    std::vector<CBigNum> private_key_sixteenths;

    for (auto public_key_sixteenth : PublicKeySixteenths())
    {
        CBigNum private_key_sixteenth = keydata[public_key_sixteenth]["privkey"];
        private_key_sixteenths.push_back(private_key_sixteenth);
    }
    return private_key_sixteenths;
}

uint160 Relay::GetSeedForDeterminingSuccessor()
{
    Relay temp_relay = *this;
    temp_relay.hashes.secret_recovery_message_hashes.resize(0);
    temp_relay.tasks.resize(0);
    temp_relay.hashes.obituary_hash = 0;
    temp_relay.hashes.goodbye_message_hash = 0;
    MemoryObject object;
    return temp_relay.GetHash160();
}

GoodbyeMessage Relay::GenerateGoodbyeMessage(Data data)
{
    GoodbyeMessage goodbye_message;
    goodbye_message.dead_relay_number = number;
    goodbye_message.successor_relay_number = data.relay_state->AssignSuccessorToRelay(this);
    goodbye_message.PopulateEncryptedKeySixteenths(data);
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
    if (n == 0)
        return Point(n);
    Point point;
    point.setvch(ParseHex(PadWithZero(n.GetHex())));
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
        recovered_point.setvch(ParseHex(prefix + PadWithZero(secret_hex)));
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

std::vector<Point> Relay::PublicKeySixteenths()
{
    return public_key_set.PublicKeySixteenths();
}

uint64_t Relay::SuccessorNumber(Data data)
{
    if (hashes.obituary_hash == 0)
        return 0;

    Obituary obituary = data.GetMessage(hashes.obituary_hash);
    return obituary.successor_number;
}

std::vector<uint64_t> Relay::KeyQuarterSharers(RelayState *relay_state)
{
    std::vector<uint64_t> sharers;

    for (auto &relay : relay_state->relays)
        if (VectorContainsEntry(relay.holders.key_quarter_holders, number))
            sharers.push_back(relay.number);

    return sharers;
}

std::vector<Relay *> Relay::QuarterHolders(Data data)
{
    std::vector<Relay *> quarter_holders;
    for (auto quarter_holder_number : holders.key_quarter_holders)
        quarter_holders.push_back(data.relay_state->GetRelayByNumber(quarter_holder_number));

    return quarter_holders;
}
