#include "KeyDistributionMessage.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"

using std::vector;

#include "log.h"
#define LOG_CATEGORY "KeyDistributionMessage.cpp"

CBigNum EncryptSecretForPublicKey(CBigNum secret, Point public_key)
{
    CBigNum shared_secret = Hash(secret * public_key);
    return secret ^ shared_secret;
}

void KeyDistributionMessage::PopulateEncryptedKeyQuarters(MemoryDataStore &keydata,
                                                          Relay &relay,
                                                          RelayState &relay_state)
{
    for (uint32_t key_quarter_position = 0; key_quarter_position < 4; key_quarter_position++)
        PopulateEncryptedKeyQuarter(keydata, relay, relay_state, key_quarter_position);
}

void KeyDistributionMessage::PopulateEncryptedKeyQuarter(MemoryDataStore &keydata,
                                                         Relay &relay,
                                                         RelayState &relay_state, uint32_t key_quarter_position)
{
    vector<CBigNum> private_key_twentyfourths = relay.PrivateKeyTwentyFourths(keydata);
    for (uint64_t i = 0; i < 6; i++)
    {
        auto key_twentyfourth_position = 6 * key_quarter_position + i;
        auto recipient = relay_state.GetRelayByNumber(relay.key_quarter_holders[key_quarter_position]);
        if (recipient == NULL)
        {
            log_ << "PopulateEncryptedKeyQuarters: no such relay\n";
            return;
        }
        auto encrypted_secret = recipient->EncryptSecret(private_key_twentyfourths[key_twentyfourth_position]);
        encrypted_key_quarters[key_quarter_position].encrypted_key_twentyfourths[i] = encrypted_secret;
    }
}

std::array<uint256, 24> KeyDistributionMessage::EncryptedKeyTwentyFourths()
{
    std::array<uint256, 24> encrypted_key_twentyfourths;
    for (uint32_t key_quarter_position = 0; key_quarter_position < 4; key_quarter_position++)
    {
        for (uint32_t i = 0; i < 6; i++)
        {
            auto key_twentyfourth_position = 6 * key_quarter_position + i;
            encrypted_key_twentyfourths[key_twentyfourth_position]
                    = encrypted_key_quarters[key_quarter_position].encrypted_key_twentyfourths[i];
        }
    }
    return encrypted_key_twentyfourths;
};

bool KeyDistributionMessage::KeyQuarterHolderPrivateKeyIsAvailable(uint64_t position, Data data,
                                                                   RelayState &relay_state, Relay &relay)
{
    auto recipients = relay_state.GetKeyQuarterHoldersAsListOf24RelayNumbers(relay.number);
    Relay *recipient = relay_state.GetRelayByNumber(recipients[position]);
    if (recipient == NULL)
        throw RelayStateException("referenced relay does not exist");
    return data.keydata[recipient->public_signing_key].HasProperty("privkey");
}

bool KeyDistributionMessage::KeyTwentyFourthIsCorrectlyEncrypted(uint64_t position, Data data,
                                                                 RelayState &relay_state, Relay &relay)
{
    auto recipients = relay_state.GetKeyQuarterHoldersAsListOf24RelayNumbers(relay.number);
    Relay *recipient = relay_state.GetRelayByNumber(recipients[position]);
    Point public_key_twentyfourth = relay.PublicKeyTwentyFourths()[position];
    CBigNum private_key_twentyfourth = data.keydata[public_key_twentyfourth]["privkey"];
    uint256 encrypted_private_key_twentyfourth = recipient->EncryptSecret(private_key_twentyfourth);
    return EncryptedKeyTwentyFourths()[position] == encrypted_private_key_twentyfourth;
}

bool KeyDistributionMessage::TryToRecoverKeyTwentyFourth(uint32_t position, Data data,
                                                         RelayState &state, Relay &relay)
{
    auto recipients = state.GetKeyQuarterHoldersAsListOf24RelayNumbers(relay.number);
    Relay *recipient = state.GetRelayByNumber(recipients[position]);
    CBigNum private_key_twentyfourth = recipient->DecryptSecret(EncryptedKeyTwentyFourths()[position],
                                                                relay.PublicKeyTwentyFourths()[position], data);
    if (private_key_twentyfourth == 0)
        return false;
    data.keydata[relay.PublicKeyTwentyFourths()[position]]["privkey"] = private_key_twentyfourth;
    return true;
}

bool KeyDistributionMessage::ValidateSizes()
{
    return true;
}

bool KeyDistributionMessage::VerifyRelayNumber(Data data)
{
    auto relay = data.relay_state->GetRelayByNumber(relay_number);
    if (relay == NULL)
        return false;
    return relay->hashes.join_message_hash == relay_join_hash;
}
