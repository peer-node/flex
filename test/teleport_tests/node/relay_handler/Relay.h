#ifndef TELEPORT_RELAY_H
#define TELEPORT_RELAY_H


#include <src/crypto/point.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "test/teleport_tests/node/relay_handler/RelayJoinMessage.h"
#include "test/teleport_tests/node/relay_handler/KeyDistributionMessage.h"
#include "test/teleport_tests/node/relay_handler/GoodbyeMessage.h"
#include "test/teleport_tests/node/relay_handler/RelayKeyHolderData.h"
#include "test/teleport_tests/node/relay_handler/RelayMessageHashData.h"


class Relay
{
public:
    uint64_t number{0};

    RelayMessageHashData hashes;
    RelayKeyHolderData holders;
    RelayPublicKeySet public_key_set;

    Point public_signing_key{SECP256K1, 0};
    bool key_distribution_message_accepted{false};
    std::vector<uint160> tasks;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(number);
        READWRITE(hashes);
        READWRITE(holders);
        READWRITE(public_key_set);
        READWRITE(public_signing_key);
        READWRITE(key_distribution_message_accepted);
        READWRITE(tasks);
    );

    JSON(number,
         hashes,
         holders,
         public_signing_key,
         key_distribution_message_accepted,
         public_key_set,
         tasks);

    HASH160();

    bool operator==(const Relay &other) const
    {
        return this->hashes.join_message_hash == other.hashes.join_message_hash;
    }

    std::vector<Point> PublicKeySixteenths();

    RelayJoinMessage GenerateJoinMessage(MemoryDataStore &keydata, uint160 mined_credit_message_hash);

    std::vector<std::vector<uint64_t> > KeyPartHolderGroups();

    std::vector<uint64_t> KeyQuarterSharers(RelayState *relay_state);

    KeyDistributionMessage GenerateKeyDistributionMessage(Data databases, uint160 encoding_message_hash,
                                                          RelayState &state);

    std::vector<CBigNum> PrivateKeySixteenths(MemoryDataStore &keydata);

    uint160 GetSeedForDeterminingSuccessor();

    GoodbyeMessage GenerateGoodbyeMessage(Data data);

    Point GenerateRecipientPublicKey(Point point_corresponding_to_secret);

    CBigNum GenerateRecipientPrivateKey(Point point_corresponding_to_secret, Data data);

    uint256 EncryptSecret(CBigNum secret);

    CBigNum DecryptSecret(uint256 encrypted_secret, Point point_corresponding_to_secret, Data data);

    uint64_t SuccessorNumber(Data data);

    CBigNum GenerateRecipientPrivateKeyQuarter(Point point_corresponding_to_secret,
                                               uint8_t key_quarter_position, Data data);

    Point GenerateRecipientPublicKeyQuarter(Point point_corresponding_to_secret, uint8_t key_quarter_position);

    uint256 EncryptSecretPoint(Point secret_point);

    Point DecryptSecretPoint(uint256 encrypted_secret, Point point_corresponding_to_secret, Data data);
};

Point DecryptPointUsingHexPrefixes(CBigNum decrypted_secret, Point point_corresponding_to_secret);

#endif //TELEPORT_RELAY_H
