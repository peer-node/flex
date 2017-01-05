#ifndef TELEPORT_RELAY_H
#define TELEPORT_RELAY_H


#include <src/crypto/point.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "RelayJoinMessage.h"
#include "KeyDistributionMessage.h"
#include "GoodbyeMessage.h"


class Relay
{
public:
    Point public_signing_key{SECP256K1, 0};
    uint64_t number{0};
    uint160 join_message_hash{0};
    uint160 mined_credit_message_hash{0};
    uint160 key_distribution_message_hash{0};
    bool key_distribution_message_accepted{false};
    std::vector<uint64_t> key_quarter_holders;
    std::vector<uint64_t> first_set_of_key_sixteenth_holders;
    std::vector<uint64_t> second_set_of_key_sixteenth_holders;
    std::vector<Point> public_key_sixteenths;
    RelayPublicKeySet public_key_set;
    std::vector<uint160> secret_recovery_message_hashes;
    uint160 goodbye_message_hash{0};
    uint160 obituary_hash{0};
    std::vector<uint160> tasks;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(public_signing_key);
        READWRITE(number);
        READWRITE(mined_credit_message_hash);
        READWRITE(join_message_hash);
        READWRITE(key_distribution_message_hash);
        READWRITE(key_distribution_message_accepted);
        READWRITE(key_quarter_holders);
        READWRITE(first_set_of_key_sixteenth_holders);
        READWRITE(second_set_of_key_sixteenth_holders);
        READWRITE(public_key_set);
        READWRITE(secret_recovery_message_hashes);
        READWRITE(goodbye_message_hash);
        READWRITE(obituary_hash);
        READWRITE(tasks);
    );

    JSON(public_signing_key, number, mined_credit_message_hash, join_message_hash, key_distribution_message_hash,
         key_distribution_message_accepted, key_quarter_holders, first_set_of_key_sixteenth_holders,
         second_set_of_key_sixteenth_holders, public_key_set, secret_recovery_message_hashes, obituary_hash,
         goodbye_message_hash, tasks);

    HASH160();

    bool operator==(const Relay &other)
    {
        return this->join_message_hash == other.join_message_hash;
    }

    std::vector<Point> PublicKeySixteenths();

    RelayJoinMessage GenerateJoinMessage(MemoryDataStore &keydata, uint160 mined_credit_message_hash);

    std::vector<std::vector<uint64_t> > KeyPartHolderGroups();

    KeyDistributionMessage
    GenerateKeyDistributionMessage(Data databases, uint160 encoding_message_hash, RelayState &state);

    std::vector<CBigNum> PrivateKeySixteenths(MemoryDataStore &keydata);

    uint160 GetSeedForDeterminingSuccessor();

    GoodbyeMessage GenerateGoodbyeMessage(Data data);

    Point GenerateRecipientPublicKey(Point point_corresponding_to_secret);

    CBigNum GenerateRecipientPrivateKey(Point point_corresponding_to_secret, Data data);

    CBigNum EncryptSecret(CBigNum secret);

    CBigNum DecryptSecret(CBigNum encrypted_secret, Point point_corresponding_to_secret, Data data);

    uint64_t SuccessorNumber(Data data);
};


#endif //TELEPORT_RELAY_H
