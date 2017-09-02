#ifndef TELEPORT_KEYDISTRIBUTIONMESSAGE_H
#define TELEPORT_KEYDISTRIBUTIONMESSAGE_H


#include <src/crypto/bignum.h>
#include <src/crypto/signature.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "RelayJoinMessage.h"
#include "test/teleport_tests/node/Data.h"
#include "test/teleport_tests/node/relays/structures/EncryptedKeyQuarter.h"

#define ALL_POSITIONS 5

class Relay;
class RelayState;

class KeyDistributionMessage
{
public:
    uint160 relay_join_hash{0};
    uint160 hash_of_message_containing_join{0};
    uint64_t relay_number{0};
    std::array<EncryptedKeyQuarter, 4> encrypted_key_quarters;
    uint8_t position{ALL_POSITIONS};
    Signature signature;

    static std::string Type() { return "key_distribution"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relay_join_hash);
        READWRITE(hash_of_message_containing_join);
        READWRITE(relay_number);
        READWRITE(encrypted_key_quarters);
        READWRITE(position);
        READWRITE(signature);
    );

    JSON(relay_join_hash, hash_of_message_containing_join, relay_number,
         encrypted_key_quarters, position, signature);

    DEPENDENCIES(relay_join_hash, hash_of_message_containing_join);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data)
    {
        RelayJoinMessage join_message = data.msgdata[relay_join_hash]["relay_join"];
        return join_message.PublicSigningKey();
    }

    void PopulateEncryptedKeyQuarters(MemoryDataStore &keydata,
                                      Relay &relay,
                                      RelayState &relay_state);

    bool KeyQuarterHolderPrivateKeyIsAvailable(uint64_t position, Data data, RelayState &relay_state, Relay &relay);

    bool TryToRecoverKeyTwentyFourth(uint32_t position, Data data, RelayState &state,
                                     Relay &relay);

    bool KeyTwentyFourthIsCorrectlyEncrypted(uint64_t position, Data data, RelayState &relay_state,
                                             Relay &relay);
    bool ValidateSizes();

    bool VerifyRelayNumber(Data data);

    std::array<uint256, 24> EncryptedKeyTwentyFourths();

    void PopulateEncryptedKeyQuarter(MemoryDataStore &keydata, Relay &relay, RelayState &relay_state, uint32_t position);
};


#endif //TELEPORT_KEYDISTRIBUTIONMESSAGE_H
