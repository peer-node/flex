#ifndef TELEPORT_KEYDISTRIBUTIONMESSAGE_H
#define TELEPORT_KEYDISTRIBUTIONMESSAGE_H


#include <src/crypto/bignum.h>
#include <src/crypto/signature.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "RelayJoinMessage.h"
#include "test/teleport_tests/node/Data.h"

class Relay;
class RelayState;

class KeyDistributionMessage
{
public:
    uint160 relay_join_hash;
    uint160 encoding_message_hash;
    uint64_t relay_number;
    std::vector<uint256> key_sixteenths_encrypted_for_key_quarter_holders;
    std::vector<uint256> key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders;
    std::vector<uint256> key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders;
    Signature signature;

    static std::string Type() { return "key_distribution"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relay_join_hash);
        READWRITE(encoding_message_hash);
        READWRITE(relay_number);
        READWRITE(key_sixteenths_encrypted_for_key_quarter_holders);
        READWRITE(key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders);
        READWRITE(key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders);
        READWRITE(signature);
    );

    JSON(relay_join_hash, encoding_message_hash, relay_number, key_sixteenths_encrypted_for_key_quarter_holders,
         key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders,
         key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders, signature);

    DEPENDENCIES(relay_join_hash, encoding_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data)
    {
        RelayJoinMessage join_message = data.msgdata[relay_join_hash]["relay_join"];
        return join_message.PublicSigningKey();
    }

    void PopulateKeySixteenthsEncryptedForKeyQuarterHolders(MemoryDataStore &keydata,
                                                            Relay &relay,
                                                            RelayState &relay_state);

    void PopulateKeySixteenthsEncryptedForKeySixteenthHolders(MemoryDataStore &keydata, Relay &relay,
                                                                        RelayState &state);

    bool KeyQuarterHolderPrivateKeyIsAvailable(uint64_t position, Data data, RelayState &relay_state, Relay &relay);

    bool KeySixteenthHolderPrivateKeyIsAvailable(uint32_t position, uint32_t first_or_second_set, Data data,
                                                 RelayState &relay_state, Relay &relay);

    bool TryToRecoverKeySixteenthEncryptedToQuarterKeyHolder(uint32_t position, Data data, RelayState &state,
                                                             Relay &relay);

    bool KeySixteenthForKeyQuarterHolderIsCorrectlyEncrypted(uint64_t position, Data data, RelayState &relay_state,
                                                             Relay &relay);

    bool TryToRecoverKeySixteenthEncryptedToKeySixteenthHolder(uint32_t position, uint32_t first_or_second_set,
                                                               Data data, RelayState &state, Relay &relay);

    bool KeySixteenthForKeySixteenthHolderIsCorrectlyEncrypted(uint64_t position, uint32_t first_or_second_set,
                                                               Data data, RelayState &relay_state, Relay &relay);

    bool AuditKeySixteenthEncryptedInRelayJoinMessage(RelayJoinMessage &join_message, uint64_t position,
                                                      MemoryDataStore &keydata);

    bool ValidateSizes();

    bool VerifyRelayNumber(Data data);
};


#endif //TELEPORT_KEYDISTRIBUTIONMESSAGE_H
