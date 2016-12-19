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
    Point public_key{SECP256K1, 0};
    uint64_t number{0};
    uint160 join_message_hash{0};
    uint160 mined_credit_message_hash{0};
    uint160 key_distribution_message_hash{0};
    bool key_distribution_message_accepted{false};
    std::vector<uint64_t> key_quarter_holders;
    std::vector<uint64_t> first_set_of_key_sixteenth_holders;
    std::vector<uint64_t> second_set_of_key_sixteenth_holders;
    std::vector<Point> public_key_sixteenths;
    std::vector<uint160> secret_recovery_message_hashes;
    uint160 goodbye_message_hash{0};
    uint160 obituary_hash{0};
    std::vector<uint160> pending_complaints_sent;
    std::vector<uint160> pending_complaints;
    std::vector<uint160> upheld_complaints;
    std::vector<uint160> refutations_of_complaints_sent;
    std::vector<uint160> tasks;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(public_key);
        READWRITE(number);
        READWRITE(mined_credit_message_hash);
        READWRITE(join_message_hash);
        READWRITE(key_distribution_message_hash);
        READWRITE(key_distribution_message_accepted);
        READWRITE(key_quarter_holders);
        READWRITE(first_set_of_key_sixteenth_holders);
        READWRITE(second_set_of_key_sixteenth_holders);
        READWRITE(secret_recovery_message_hashes);
        READWRITE(goodbye_message_hash);
        READWRITE(obituary_hash);
        READWRITE(pending_complaints);
        READWRITE(pending_complaints_sent);
        READWRITE(upheld_complaints);
        READWRITE(refutations_of_complaints_sent);
        READWRITE(tasks);
    );

    JSON(public_key, number, mined_credit_message_hash, join_message_hash, key_distribution_message_hash,
         key_distribution_message_accepted, key_quarter_holders, first_set_of_key_sixteenth_holders,
         second_set_of_key_sixteenth_holders, secret_recovery_message_hashes, obituary_hash, goodbye_message_hash,
         pending_complaints, pending_complaints_sent, upheld_complaints, refutations_of_complaints_sent, tasks);

    HASH160();

    bool operator==(const Relay &other)
    {
        return this->join_message_hash == other.join_message_hash;
    }

    RelayJoinMessage GenerateJoinMessage(MemoryDataStore &keydata, uint160 mined_credit_message_hash);

    std::vector<std::vector<uint64_t> > KeyPartHolderGroups();

    KeyDistributionMessage
    GenerateKeyDistributionMessage(Data databases, uint160 encoding_message_hash, RelayState &state);

    std::vector<CBigNum> PrivateKeySixteenths(MemoryDataStore &keydata);

    uint160 GetSeedForDeterminingSuccessor();

    GoodbyeMessage GenerateGoodbyeMessage(Data data);


};


#endif //TELEPORT_RELAY_H
