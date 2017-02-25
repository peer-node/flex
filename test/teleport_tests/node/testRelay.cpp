#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/relay_handler/RelayState.h"
#include "CreditSystem.h"


using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"

static MemoryDataStore keydata;

class ARelay : public Test
{
public:
    MemoryDataStore msgdata, creditdata;
    CreditSystem *credit_system;
    Data *data;
    Relay relay;
    RelayState relay_state;
    MinedCreditMessage msg;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        data = new Data(msgdata, creditdata, keydata, &relay_state);
        msg.mined_credit.keydata = Point(CBigNum(5)).getvch();
        keydata[Point(CBigNum(5))]["privkey"] = CBigNum(5);
        msg.mined_credit.network_state.batch_number = 1;
        credit_system->StoreMinedCreditMessage(msg);
    }

    virtual void TearDown()
    {
        delete data;
        delete credit_system;
    }
};

TEST_F(ARelay, GeneratesARelayJoinMessageWithAValidSignature)
{
    RelayJoinMessage relay_join_message = relay.GenerateJoinMessage(keydata, msg.GetHash160());
    relay_join_message.Sign(*data);
    bool ok = relay_join_message.VerifySignature(*data);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ARelay, HasItsPublicKeySetDeterminedWhenTheRelayStateProcessesItsJoinMessage)
{
    RelayJoinMessage relay_join_message = relay.GenerateJoinMessage(keydata, msg.GetHash160());
    relay_state.ProcessRelayJoinMessage(relay_join_message);

    ASSERT_THAT(relay_state.relays[0].PublicKeySixteenths().size(), Eq(16));

    for (uint64_t i = 0; i < 16; i++)
        ASSERT_THAT(relay_state.relays[0].PublicKeySixteenths()[i],
                    Eq(relay_join_message.public_key_set.PublicKeySixteenths()[i]));
}


class ARelayWithAPublicKeySet : public ARelay
{
public:
    virtual void SetUp()
    {
        ARelay::SetUp();
        RelayJoinMessage relay_join_message = relay.GenerateJoinMessage(keydata, msg.GetHash160());
        relay_state.ProcessRelayJoinMessage(relay_join_message);
        relay = relay_state.relays[0];
    }

    virtual void TearDown()
    {
        ARelay::TearDown();
    }
};

CBigNum RandomSecret()
{
    CBigNum secret;
    secret.Randomize(Secp256k1Point::Modulus());
    return secret;
}

TEST_F(ARelayWithAPublicKeySet,
       GeneratesAMatchingRecipientPublicAndPrivateKeyForASecretGivenTheCorrespondingPoint)
{
    CBigNum secret = RandomSecret();
    Point point_corresponding_to_secret(secret);

    Point recipient_public_key = relay.GenerateRecipientPublicKey(point_corresponding_to_secret);
    CBigNum recipient_private_key = relay.GenerateRecipientPrivateKey(point_corresponding_to_secret, *data);

    ASSERT_THAT(Point(recipient_private_key), Eq(recipient_public_key));
}

TEST_F(ARelayWithAPublicKeySet, EncryptsASecretWhichCanBeDecryptedUsingTheRecipientPrivateKey)
{
    CBigNum secret = RandomSecret();
    Point point_corresponding_to_secret(secret);

    uint256 encrypted_secret = relay.EncryptSecret(secret);
    CBigNum recipient_private_key = relay.GenerateRecipientPrivateKey(point_corresponding_to_secret, *data);

    CBigNum shared_secret = Hash(recipient_private_key * point_corresponding_to_secret);
    CBigNum decrypted_secret = CBigNum(encrypted_secret) ^ shared_secret;

    ASSERT_THAT(decrypted_secret, Eq(secret));
}

TEST_F(ARelayWithAPublicKeySet, DecryptsASecretWhichWasEncryptedUsingTheRecipientPublicKey)
{
    CBigNum secret = RandomSecret();
    Point point_corresponding_to_secret(secret);

    uint256 encrypted_secret = relay.EncryptSecret(secret);
    CBigNum decrypted_secret = relay.DecryptSecret(encrypted_secret, point_corresponding_to_secret, *data);

    ASSERT_THAT(decrypted_secret, Eq(secret));
}

class ARelayWithKeyPartHoldersAssigned : public ARelay
{
public:
    Relay *relay;
    uint160 encoding_message_hash{5};

    virtual void SetUp()
    {
        ARelay::SetUp();
        static RelayState persistent_relay_state;
        static MemoryDataStore persistent_keydata;
        static MemoryDataStore persistent_msgdata;
        if (persistent_relay_state.relays.size() == 0)
        {
            DoFirstSetUp();
            persistent_relay_state = relay_state;
            persistent_keydata = keydata;
            persistent_msgdata = msgdata;
        }
        else
        {
            relay_state = persistent_relay_state;
            keydata = persistent_keydata;
            msgdata = persistent_msgdata;
        }
        relay = relay_state.GetRelayByNumber(TheNumberOfARelayWhichIsAQuarterKeyHolderAndHasKeyPartHoldersAssigned());
    }

    virtual void DoFirstSetUp()
    {
        for (uint32_t i = 1; i <= 37; i ++)
            AddARelayToTheRelayState(i);

        for (uint32_t i = 1; i <= 37; i ++)
            relay_state.AssignKeyPartHoldersToRelay(relay_state.relays[i - 1], i + 1);

    }

    uint64_t TheNumberOfARelayWhichIsAQuarterKeyHolderAndHasKeyPartHoldersAssigned()
    {
        for (Relay &relay : relay_state.relays)
        {
            uint64_t candidate = relay.holders.key_quarter_holders[1];
            if (relay_state.GetRelayByNumber(candidate)->holders.key_quarter_holders.size() > 0)
                return candidate;
        }
        return 0;
    }

    virtual void AddARelayToTheRelayState(uint64_t random_seed)
    {
        RelayJoinMessage join_message = Relay().GenerateJoinMessage(keydata, random_seed);
        data->StoreMessage(join_message);
        relay_state.ProcessRelayJoinMessage(join_message);
    }

    virtual void TearDown()
    {
        ARelay::TearDown();
    }
};




TEST_F(ARelayWithKeyPartHoldersAssigned, GeneratesAKeyDistributionMessageWithAValidSignature)
{
    KeyDistributionMessage key_distribution_message = relay->GenerateKeyDistributionMessage(*data,
                                                                                            encoding_message_hash,
                                                                                            relay_state);
    key_distribution_message.Sign(*data);
    bool ok = key_distribution_message.VerifySignature(*data);
    ASSERT_THAT(ok, Eq(true));
}

uint256 EncryptSecretForRelay(CBigNum secret, RelayState &state, uint64_t relay_number)
{
    Relay *relay = state.GetRelayByNumber(relay_number);

    return relay->EncryptSecret(secret);
}

TEST_F(ARelayWithKeyPartHoldersAssigned,
        GeneratesAKeyDistributionMessageWhosePublicKeySetPassesVerificationByKeyPartHolders)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);

    bool ok = relay->public_key_set.VerifyGeneratedPoints(keydata);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ARelayWithKeyPartHoldersAssigned,
       GeneratesAKeyDistributionMessageWhosePublicKeySetFailsVerificationIfThePrivateKeyPartsAreBad)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
    relay->public_key_set.key_points[1][2] += Point(CBigNum(1));
    bool ok = relay->public_key_set.VerifyGeneratedPoints(keydata);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(ARelayWithKeyPartHoldersAssigned, GeneratesAKeyDistributionMessageWhichRevealsKeyPartsToQuarterKeyHolders)
{
    KeyDistributionMessage key_distribution_message = relay->GenerateKeyDistributionMessage(*data,
                                                                                            encoding_message_hash,
                                                                                            relay_state);
    ASSERT_THAT(key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders.size(), Eq(16));
    for (uint64_t i = 0; i < 16; i ++)
    {
        CBigNum private_key_sixteenth = keydata[relay->PublicKeySixteenths()[i]]["privkey"];
        Relay *recipient = relay_state.GetRelayByNumber(relay->holders.key_quarter_holders[i / 4]);
        uint256 encrypted_private_key_sixteenth = recipient->EncryptSecret(private_key_sixteenth);

        ASSERT_THAT(key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders[i],
                    Eq(encrypted_private_key_sixteenth));
    }
}

TEST_F(ARelayWithKeyPartHoldersAssigned, GeneratesAKeyDistributionMessageWhichRevealsKeyPartsToKeySixteenthHolders)
{
    KeyDistributionMessage key_distribution_message = relay->GenerateKeyDistributionMessage(*data,
                                                                                            encoding_message_hash,
                                                                                            relay_state);
    ASSERT_THAT(key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders.size(), Eq(16));
    ASSERT_THAT(key_distribution_message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders.size(), Eq(16));
    for (uint64_t i = 0; i < 16; i ++)
    {
        CBigNum private_key_sixteenth = keydata[relay->PublicKeySixteenths()[i]]["privkey"];

        ASSERT_THAT(key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[i],
                    Eq(EncryptSecretForRelay(private_key_sixteenth, relay_state,
                                             relay->holders.first_set_of_key_sixteenth_holders[i])));
        ASSERT_THAT(key_distribution_message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[i],
                    Eq(EncryptSecretForRelay(private_key_sixteenth, relay_state,
                                             relay->holders.second_set_of_key_sixteenth_holders[i])));
    }
}

TEST_F(ARelayWithKeyPartHoldersAssigned, GeneratesAKeyDistributionMessageForWhichThePrivateKeySixteenthsAreAvailable)
{
    KeyDistributionMessage key_distribution_message = relay->GenerateKeyDistributionMessage(*data,
                                                                                            encoding_message_hash,
                                                                                            relay_state);
    for (uint32_t position = 0; position < 16; position++)
    {
        bool key_present = key_distribution_message.KeyQuarterHolderPrivateKeyIsAvailable(position, *data,
                                                                                          relay_state, *relay);
        ASSERT_THAT(key_present, Eq(true));
        for (uint32_t first_or_second_set = 1; first_or_second_set <=2 ; first_or_second_set++)
        {
            key_present = key_distribution_message.KeySixteenthHolderPrivateKeyIsAvailable(position,
                                                                                           first_or_second_set,
                                                                                           *data, relay_state, *relay);
            ASSERT_THAT(key_present, Eq(true));
        }
    }
}

TEST_F(ARelayWithKeyPartHoldersAssigned, GeneratesAKeyDistributionMessageFromWhichPrivateKeySixteenthsCanBeRecovered)
{
    KeyDistributionMessage key_distribution_message = relay->GenerateKeyDistributionMessage(*data,
                                                                                            encoding_message_hash,
                                                                                            relay_state);
    for (uint32_t position = 0; position < 16; position++)
    {
        bool recovered = key_distribution_message.TryToRecoverKeySixteenthEncryptedToQuarterKeyHolder(position, *data,
                                                                                                      relay_state, *relay);
        ASSERT_THAT(recovered, Eq(true));
        recovered = key_distribution_message.TryToRecoverKeySixteenthEncryptedToKeySixteenthHolder(position, 1, *data,
                                                                                                   relay_state, *relay);
        ASSERT_THAT(recovered, Eq(true));
        recovered = key_distribution_message.TryToRecoverKeySixteenthEncryptedToKeySixteenthHolder(position, 2, *data,
                                                                                                   relay_state, *relay);
        ASSERT_THAT(recovered, Eq(true));
    }
}

TEST_F(ARelayWithKeyPartHoldersAssigned, GeneratesAKeyDistributionMessageWhichAuditsTheEncryptedKeyParts)
{
    KeyDistributionMessage key_distribution_message = relay->GenerateKeyDistributionMessage(*data,
                                                                                            encoding_message_hash,
                                                                                            relay_state);
    for (uint32_t position = 0; position < 16; position++)
    {
        ASSERT_TRUE(key_distribution_message.KeySixteenthForKeyQuarterHolderIsCorrectlyEncrypted(position, *data,
                                                                                                 relay_state, *relay));
        ASSERT_TRUE(key_distribution_message.KeySixteenthForKeySixteenthHolderIsCorrectlyEncrypted(position, 1, *data,
                                                                                                   relay_state, *relay));
        ASSERT_TRUE(key_distribution_message.KeySixteenthForKeySixteenthHolderIsCorrectlyEncrypted(position, 2, *data,
                                                                                                   relay_state, *relay));
    }
}

TEST_F(ARelayWithKeyPartHoldersAssigned, GeneratesAKeyDistributionMessageWhichDetectsBadEncryptedKeyParts)
{
    KeyDistributionMessage key_distribution_message = relay->GenerateKeyDistributionMessage(*data,
                                                                                            encoding_message_hash,
                                                                                            relay_state);
    for (uint32_t position = 0; position < 16; position++)
    {
        key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders[position] += 1;
        ASSERT_FALSE(key_distribution_message.KeySixteenthForKeyQuarterHolderIsCorrectlyEncrypted(position, *data,
                                                                                                  relay_state, *relay));
        key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[position] += 1;
        ASSERT_FALSE(key_distribution_message.KeySixteenthForKeySixteenthHolderIsCorrectlyEncrypted(position, 1, *data,
                                                                                                    relay_state, *relay));
        key_distribution_message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[position] += 1;
        ASSERT_FALSE(key_distribution_message.KeySixteenthForKeySixteenthHolderIsCorrectlyEncrypted(position, 2, *data,
                                                                                                    relay_state, *relay));
    }
}

TEST_F(ARelayWithKeyPartHoldersAssigned,
       GeneratesAKeyDistributionMessageWhichAuditsTheEncryptedKeyPartsInTheRelayJoinMessage)
{
    KeyDistributionMessage key_distribution_message = relay->GenerateKeyDistributionMessage(*data,
                                                                                            encoding_message_hash,
                                                                                            relay_state);
    RelayJoinMessage join_message = msgdata[key_distribution_message.relay_join_hash]["relay_join"];
    for (uint32_t position = 0; position < 16; position++)
    {
        bool ok = key_distribution_message.AuditKeySixteenthEncryptedInRelayJoinMessage(join_message, position, keydata);
        ASSERT_THAT(ok, Eq(true));
    }
}

TEST_F(ARelayWithKeyPartHoldersAssigned,
       GeneratesAKeyDistributionMessageWhichDetectsBadEncryptedKeyPartsInTheRelayJoinMessage)
{
    KeyDistributionMessage key_distribution_message = relay->GenerateKeyDistributionMessage(*data,
                                                                                            encoding_message_hash,
                                                                                            relay_state);
    RelayJoinMessage join_message = msgdata[key_distribution_message.relay_join_hash]["relay_join"];
    for (uint32_t position = 0; position < 16; position++)
    {
        join_message.encrypted_private_key_sixteenths[position] += 1;
        bool ok = key_distribution_message.AuditKeySixteenthEncryptedInRelayJoinMessage(join_message, position, keydata);
        ASSERT_THAT(ok, Eq(false));
    }
}


class ARelayInARelayStateWhichHasProcessedItsKeyDistributionMessage : public ARelayWithKeyPartHoldersAssigned
{
public:
    virtual void SetUp()
    {
        ARelayWithKeyPartHoldersAssigned::SetUp();
        auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash,
                                                                              relay_state);
        relay_state.ProcessKeyDistributionMessage(key_distribution_message);
    }

    virtual void TearDown()
    {
        ARelayWithKeyPartHoldersAssigned::TearDown();
    }
};

TEST_F(ARelayInARelayStateWhichHasProcessedItsKeyDistributionMessage,
       GeneratesAGoodbyeMessageWithTheSameSuccessorAsItsObituary)
{
    GoodbyeMessage goodbye = relay->GenerateGoodbyeMessage(*data);
    Obituary obituary = relay_state.GenerateObituary(relay, SAID_GOODBYE);
    ASSERT_THAT(goodbye.successor_number, Eq(obituary.successor_number));
}

TEST_F(ARelayInARelayStateWhichHasProcessedItsKeyDistributionMessage,
       GeneratesAGoodbyeMessageWithAValidSignature)
{
    GoodbyeMessage goodbye = relay->GenerateGoodbyeMessage(*data);
    goodbye.Sign(*data);
    ASSERT_TRUE(goodbye.VerifySignature(*data));
}

TEST_F(ARelayInARelayStateWhichHasProcessedItsKeyDistributionMessage,
       GeneratesAGoodbyeMessageWhichEncryptsHeldKeyQuartersForTheSuccessor)
{
    GoodbyeMessage goodbye = relay->GenerateGoodbyeMessage(*data);
    ASSERT_THAT(goodbye.key_quarter_sharers.size(), Gt(0));

    for (uint32_t i = 0; i < goodbye.key_quarter_sharers.size(); i++)
    {
        Relay *key_sharer = relay_state.GetRelayByNumber(goodbye.key_quarter_sharers[i]);
        ASSERT_FALSE(key_sharer == NULL);
        ASSERT_THAT(goodbye.encrypted_key_sixteenths[i].size(), Eq(4));
        for (uint32_t j = 0; j < 4; j++)
        {
            Point public_key_sixteenth = key_sharer->PublicKeySixteenths()[goodbye.key_quarter_positions[i] * 4 + j];
            CBigNum private_key_sixteenth = keydata[public_key_sixteenth]["privkey"];
            ASSERT_THAT(goodbye.encrypted_key_sixteenths[i][j],
                        Eq(EncryptSecretForRelay(private_key_sixteenth, relay_state, goodbye.successor_number)));
        }
    }
}