#include <src/crypto/secp256k1point.h>
#include <src/crypto/bignum_hashes.h>
#include "Relay.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "Relay.cpp"


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
    if (key_quarter_holders.size() == 0)
        relay_state.AssignKeyPartHoldersToRelay(*this, encoding_message_hash);

    KeyDistributionMessage key_distribution_message;
    key_distribution_message.encoding_message_hash = encoding_message_hash;
    key_distribution_message.relay_join_hash = join_message_hash;
    key_distribution_message.relay_number = number;
    key_distribution_message.PopulateKeySixteenthsEncryptedForKeyQuarterHolders(data.keydata, *this, relay_state);
    key_distribution_message.PopulateKeySixteenthsEncryptedForKeySixteenthHolders(data.keydata, *this, relay_state);
    key_distribution_message.Sign(data);
    return key_distribution_message;
}

std::vector<std::vector<uint64_t> > Relay::KeyPartHolderGroups()
{
    return std::vector<std::vector<uint64_t> >{key_quarter_holders,
                                               first_set_of_key_sixteenth_holders,
                                               second_set_of_key_sixteenth_holders};
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
    temp_relay.secret_recovery_message_hashes.resize(0);
    temp_relay.tasks.resize(0);
    temp_relay.obituary_hash = 0;
    temp_relay.goodbye_message_hash = 0;
    return temp_relay.GetHash160();
}

GoodbyeMessage Relay::GenerateGoodbyeMessage(Data data)
{
    GoodbyeMessage goodbye_message;
    goodbye_message.dead_relay_number = number;
    goodbye_message.successor_relay_number = data.relay_state->AssignSuccessorToRelay(this);
    goodbye_message.PopulateEncryptedKeySixteenths(data);
    return goodbye_message;
}

Point Relay::GenerateRecipientPublicKey(Point point_corresponding_to_secret)
{
    return public_key_set.GenerateReceivingPublicKey(point_corresponding_to_secret);
}

CBigNum Relay::GenerateRecipientPrivateKey(Point point_corresponding_to_secret, Data data)
{
    return public_key_set.GenerateReceivingPrivateKey(point_corresponding_to_secret, data.keydata);
}

CBigNum Relay::EncryptSecret(CBigNum secret)
{
    Point point_corresponding_to_secret = Point(secret);
    Point recipient_public_key = GenerateRecipientPublicKey(point_corresponding_to_secret);
    CBigNum shared_secret = Hash(secret * recipient_public_key);

    return secret ^ shared_secret;
}

CBigNum Relay::DecryptSecret(CBigNum encrypted_secret, Point point_corresponding_to_secret, Data data)
{
    CBigNum recipient_private_key = GenerateRecipientPrivateKey(point_corresponding_to_secret, data);
    CBigNum shared_secret = Hash(recipient_private_key * point_corresponding_to_secret);
    CBigNum recovered_secret = encrypted_secret ^ shared_secret;

    return Point(recovered_secret) == point_corresponding_to_secret ? recovered_secret : 0;
}

std::vector<Point> Relay::PublicKeySixteenths()
{
    return public_key_set.PublicKeySixteenths();
}

uint64_t Relay::SuccessorNumber(Data data)
{
    if (obituary_hash == 0)
        return 0;

    Obituary obituary = data.GetMessage(obituary_hash);
    return obituary.successor_number;
}
