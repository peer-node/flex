#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"
#include "test/teleport_tests/node/credit/structures/CreditSystem.h"
#include "test/teleport_tests/node/relays/structures/RelayMemoryCache.h"


using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ARelay : public Test
{
public:
    MemoryDataStore msgdata, creditdata, keydata, depositdata;
    CreditSystem *credit_system;
    Data *data;
    Relay relay;
    RelayState relay_state;
    MinedCreditMessage msg;

    virtual void SetUp()
    {
        data = new Data(msgdata, creditdata, keydata, depositdata, &relay_state);
        credit_system = new CreditSystem(*data);

        data->CreateCache();
        msg.mined_credit.keydata = Point(CBigNum(5)).getvch();
        keydata[Point(CBigNum(5))]["privkey"] = CBigNum(5);
        msg.mined_credit.network_state.batch_number = 1;
        credit_system->StoreMinedCreditMessage(msg);
    }

    virtual void TearDown()
    {
        delete credit_system;
        delete data;
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

    ASSERT_THAT(relay_state.relays[0].PublicKeyTwentyFourths().size(), Eq(24));

    for (uint64_t i = 0; i < 24; i++)
        ASSERT_THAT(relay_state.relays[0].PublicKeyTwentyFourths()[i],
                    Eq(relay_join_message.public_key_set.PublicKeyTwentyFourths()[i]));
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

#define DECLARE_CACHED_DATA()                                                                  \
 static MemoryDataStore reference_msgdata, reference_creditdata, reference_keydata;            \
 static RelayMemoryCache reference_cache;                                                      \
 static RelayState reference_relay_state;                                                      \
 static uint64_t reference_relay_number{0};

#define SAVE_CACHED_DATA()                                                                     \
{reference_msgdata = msgdata; reference_creditdata = creditdata; reference_keydata = keydata;  \
 reference_relay_state = relay_state; reference_cache = *data->cache;                          \
 reference_relay_number = relay->number;}

#define LOAD_CACHED_DATA()                                                                     \
{msgdata = reference_msgdata; creditdata = reference_creditdata; keydata = reference_keydata;  \
 relay_state = reference_relay_state; *data->cache = reference_cache;                          \
 relay = relay_state.GetRelayByNumber(reference_relay_number);}


#define DECLARE_CACHED_VARIABLE(y, x) static y reference_ ## x;
#define SAVE_CACHED_VARIABLE(x) reference_ ## x = x;
#define SAVE_CACHED_VARIABLES(a, b) SAVE_CACHED_VARIABLE(a) SAVE_CACHED_VARIABLE(b)
#define LOAD_CACHED_VARIABLE(x) x = reference_ ## x;
#define LOAD_CACHED_VARIABLES(a, b) LOAD_CACHED_VARIABLE(a) LOAD_CACHED_VARIABLE(b)

#define FIRST_SETUP() reference_relay_number == 0

class ARelayWithKeyQuarterHoldersAssigned : public ARelay
{
public:
    Relay *relay;
    uint160 encoding_message_hash{5};

    virtual void SetUp()
    {
        ARelay::SetUp();

        DoCachedSetUpForARelayWithKeyQuarterHoldersAssigned();
    }

    void DoCachedSetUpForARelayWithKeyQuarterHoldersAssigned()
    {
        DECLARE_CACHED_DATA();

        if (FIRST_SETUP())
        {
            DoSetUpForARelayWithKeyQuarterHoldersAssigned();
            SAVE_CACHED_DATA();
        }
        else
        {
            LOAD_CACHED_DATA();
        }
        relay = relay_state.GetRelayByNumber(TheNumberOfARelayWhichIsAKeyQuarterHolderAndHasKeyQuarterHoldersAssigned());
    }

    virtual void DoSetUpForARelayWithKeyQuarterHoldersAssigned()
    {
        for (uint32_t i = 1; i <= 53; i ++)
            AddARelayToTheRelayState(i);

        relay = relay_state.GetRelayByNumber(TheNumberOfARelayWhichIsAKeyQuarterHolderAndHasKeyQuarterHoldersAssigned());
    }

    uint64_t TheNumberOfARelayWhichIsAKeyQuarterHolderAndHasKeyQuarterHoldersAssigned()
    {
        for (Relay &relay : relay_state.relays)
        {
            uint64_t candidate = relay.key_quarter_holders[1];
            if (relay_state.GetRelayByNumber(candidate)->HasFourKeyQuarterHolders())
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

TEST_F(ARelayWithKeyQuarterHoldersAssigned, GeneratesAKeyDistributionMessageWithAValidSignature)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
    key_distribution_message.Sign(*data);
    bool ok = key_distribution_message.VerifySignature(*data);
    ASSERT_THAT(ok, Eq(true));
}

uint256 EncryptSecretForRelay(CBigNum secret, RelayState &state, uint64_t relay_number)
{
    Relay *relay = state.GetRelayByNumber(relay_number);
    return relay->EncryptSecret(secret);
}

TEST_F(ARelayWithKeyQuarterHoldersAssigned,
        GeneratesAKeyDistributionMessageWhosePublicKeySetPassesVerificationByKeyQuarterHolders)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
    bool ok = relay->public_key_set.VerifyGeneratedPoints(keydata);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ARelayWithKeyQuarterHoldersAssigned,
       GeneratesAKeyDistributionMessageWhosePublicKeySetFailsVerificationIfThePrivateKeyPartsAreBad)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
    relay->public_key_set.key_points[1][2] += Point(CBigNum(1));
    bool ok = relay->public_key_set.VerifyGeneratedPoints(keydata);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(ARelayWithKeyQuarterHoldersAssigned, GeneratesAKeyDistributionMessageWhichRevealsKeyPartsToKeyQuarterHolders)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
    for (uint64_t i = 0; i < 24; i ++)
    {
        CBigNum private_key_twentyfourth = keydata[relay->PublicKeyTwentyFourths()[i]]["privkey"];
        Relay *recipient = relay_state.GetRelayByNumber(relay->key_quarter_holders[i / 6]);
        uint256 encrypted_private_key_twentyfourth = recipient->EncryptSecret(private_key_twentyfourth);

        ASSERT_THAT(key_distribution_message.EncryptedKeyTwentyFourths()[i],
                    Eq(encrypted_private_key_twentyfourth));
    }
}

TEST_F(ARelayWithKeyQuarterHoldersAssigned, GeneratesAKeyDistributionMessageForWhichThePrivateKeyTwentyFourthsAreAvailable)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
    for (uint32_t position = 0; position < 24; position++)
    {
        bool key_present = key_distribution_message.KeyQuarterHolderPrivateKeyIsAvailable(position, *data,
                                                                                          relay_state, *relay);
        ASSERT_THAT(key_present, Eq(true));
    }
}

TEST_F(ARelayWithKeyQuarterHoldersAssigned, GeneratesAKeyDistributionMessageFromWhichPrivateKeyTwentyFourthsCanBeRecovered)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
    for (uint32_t position = 0; position < 24; position++)
    {
        bool recovered = key_distribution_message.TryToRecoverKeyTwentyFourth(position, *data, relay_state, *relay);
        ASSERT_THAT(recovered, Eq(true));
    }
}

TEST_F(ARelayWithKeyQuarterHoldersAssigned, GeneratesAKeyDistributionMessageWhichDetectsGoodEncryptedKeyParts)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
    for (uint32_t position = 0; position < 24; position++)
    {
        ASSERT_TRUE(key_distribution_message.KeyTwentyFourthIsCorrectlyEncrypted(position, *data, relay_state, *relay));
    }
}

TEST_F(ARelayWithKeyQuarterHoldersAssigned, GeneratesAKeyDistributionMessageWhichDetectsBadEncryptedKeyParts)
{
    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
    for (uint32_t position = 0; position < 24; position++)
    {
        key_distribution_message.encrypted_key_quarters[position / 6].encrypted_key_twentyfourths[position % 6] += 1;
        ASSERT_FALSE(key_distribution_message.KeyTwentyFourthIsCorrectlyEncrypted(position, *data, relay_state, *relay));
    }
}


class ARelayInARelayStateWhichHasProcessedItsKeyDistributionMessage : public ARelayWithKeyQuarterHoldersAssigned
{
public:
    KeyDistributionMessage key_distribution_message;

    virtual void SetUp()
    {
        ARelayWithKeyQuarterHoldersAssigned::SetUp();
        key_distribution_message = relay->GenerateKeyDistributionMessage(*data, encoding_message_hash, relay_state);
        data->StoreMessage(key_distribution_message);
        relay_state.ProcessKeyDistributionMessage(key_distribution_message);
    }

    virtual void TearDown()
    {
        ARelayWithKeyQuarterHoldersAssigned::TearDown();
    }

    uint160 HashOfMessageContainingRelayState()
    {
        MinedCreditMessage msg;
        msg.mined_credit.network_state.relay_state_hash = relay_state.GetHash160();
        data->cache->Store(relay_state);
        data->StoreMessage(msg);
        return msg.GetHash160();
    }
};


class ARelaysKeyDistributionAuditMessage : public ARelayInARelayStateWhichHasProcessedItsKeyDistributionMessage
{
public:
    KeyDistributionAuditMessage audit_message;
    uint160 hash_of_message_containing_relay_state;

    virtual void SetUp()
    {
        ARelayInARelayStateWhichHasProcessedItsKeyDistributionMessage::SetUp();
        DoCachedSetUpForARelaysKeyDistributionAuditMessage();
    }

    void DoCachedSetUpForARelaysKeyDistributionAuditMessage()
    {
        DECLARE_CACHED_DATA();
        DECLARE_CACHED_VARIABLE(KeyDistributionAuditMessage, audit_message);
        DECLARE_CACHED_VARIABLE(uint160, hash_of_message_containing_relay_state);

        auto relay_number = relay->number;

        if (FIRST_SETUP())
        {
            DoSetUpForARelaysKeyDistributionAuditMessage();

            SAVE_CACHED_DATA();
            SAVE_CACHED_VARIABLES(audit_message, hash_of_message_containing_relay_state);
        }
        else
        {
            LOAD_CACHED_DATA();
            LOAD_CACHED_VARIABLES(audit_message, hash_of_message_containing_relay_state);
        }
    }

    void DoSetUpForARelaysKeyDistributionAuditMessage()
    {
        hash_of_message_containing_relay_state = HashOfMessageContainingRelayState();
        audit_message = relay->GenerateKeyDistributionAuditMessage(hash_of_message_containing_relay_state, *data);
        data->StoreMessage(audit_message);
    }

    virtual void TearDown()
    {
        ARelayInARelayStateWhichHasProcessedItsKeyDistributionMessage::TearDown();
    }

    void CorruptKeyDistributionMessage(uint64_t position);
};

TEST_F(ARelaysKeyDistributionAuditMessage, HasAValidSignature)
{
    ASSERT_TRUE(audit_message.VerifySignature(*data));
}

TEST_F(ARelaysKeyDistributionAuditMessage, IdentifiesTheCorrectAuditorSelection)
{
    auto auditor_selection_from_message = audit_message.GetAuditorSelection(*data);

    RelayState state_from_which_selection_is_generated = relay_state;
    state_from_which_selection_is_generated.latest_mined_credit_message_hash = hash_of_message_containing_relay_state;
    KeyDistributionAuditorSelection auditor_selection;
    auditor_selection.Populate(relay, &state_from_which_selection_is_generated);
    ASSERT_THAT(auditor_selection_from_message, Eq(auditor_selection));
}

TEST_F(ARelaysKeyDistributionAuditMessage, ContainsTheKeyTwentyFourthsEncryptedToTheAuditors)
{
    auto auditor_selection = audit_message.GetAuditorSelection(*data);

    for (uint64_t i = 0; i < 24; i++)
    {
        auto encrypted_secret = audit_message.first_set_of_encrypted_key_twentyfourths[i];
        auto recipient = relay_state.GetRelayByNumber(auditor_selection.first_set_of_auditors[i]);
        auto private_key_twentyfourth = relay->PrivateKeyTwentyFourths(keydata)[i];
        ASSERT_THAT(encrypted_secret, Eq(recipient->EncryptSecret(private_key_twentyfourth)));

        encrypted_secret = audit_message.second_set_of_encrypted_key_twentyfourths[i];
        recipient = relay_state.GetRelayByNumber(auditor_selection.second_set_of_auditors[i]);
        ASSERT_THAT(encrypted_secret, Eq(recipient->EncryptSecret(private_key_twentyfourth)));
    }
}

TEST_F(ARelaysKeyDistributionAuditMessage, DetectsIfTheEnclosedSecretsAreCorrect)
{
    bool ok = audit_message.EnclosedSecretsAreCorrect(*data);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ARelaysKeyDistributionAuditMessage, DetectsIfTheEnclosedSecretsAreIncorrect)
{
    audit_message.second_set_of_encrypted_key_twentyfourths[3] += 1;
    bool ok = audit_message.EnclosedSecretsAreCorrect(*data);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(ARelaysKeyDistributionAuditMessage, GeneratesAComplaintIfTheEnclosedSecretsAreIncorrect)
{
    audit_message.second_set_of_encrypted_key_twentyfourths[3] += 1;
    data->StoreMessage(audit_message);

    KeyDistributionAuditComplaint complaint = audit_message.GenerateComplaint(*data);
    ASSERT_THAT(complaint.set_of_encrypted_key_twentyfourths, Eq(2));
    ASSERT_THAT(complaint.position_of_bad_encrypted_key_twentyfourth, Eq(3));
}

TEST_F(ARelaysKeyDistributionAuditMessage, GeneratesAComplaintWithTheCorrectPrivateReceivingKey)
{
    audit_message.second_set_of_encrypted_key_twentyfourths[3] += 1;
    data->StoreMessage(audit_message);
    auto auditor_selection = audit_message.GetAuditorSelection(*data);
    auto auditor = relay_state.GetRelayByNumber(auditor_selection.second_set_of_auditors[3]);
    Point public_key_twentyfourth = relay->PublicKeyTwentyFourths()[3];
    auto private_receiving_key = auditor->GenerateRecipientPrivateKey(public_key_twentyfourth, *data).getuint256();

    KeyDistributionAuditComplaint complaint = audit_message.GenerateComplaint(*data);

    ASSERT_THAT(complaint.private_receiving_key, Eq(private_receiving_key));
}

TEST_F(ARelaysKeyDistributionAuditMessage, GeneratesAComplaintWhichVerifiesTheEncryptedSecretWasBad)
{
    audit_message.second_set_of_encrypted_key_twentyfourths[3] += 1;
    data->StoreMessage(audit_message);
    KeyDistributionAuditComplaint complaint = audit_message.GenerateComplaint(*data);

    bool good_complaint = complaint.VerifyEncryptedSecretWasBad(*data);
    ASSERT_THAT(good_complaint, Eq(true));
}

TEST_F(ARelaysKeyDistributionAuditMessage, FailsAComplaintsVerificiationThatTheSecretWasBadIfItWasGood)
{
    KeyDistributionAuditComplaint complaint;
    complaint.audit_message_hash = audit_message.GetHash160();
    complaint.position_of_bad_encrypted_key_twentyfourth = 3;
    complaint.set_of_encrypted_key_twentyfourths = 2;
    complaint.PopulatePrivateReceivingKey(*data);

    bool good_complaint = complaint.VerifyEncryptedSecretWasBad(*data);
    ASSERT_THAT(good_complaint, Eq(false));
}

TEST_F(ARelaysKeyDistributionAuditMessage, GeneratesAComplaintWhichDetectsIfThePrivateReceivingKeyIsCorrect)
{
    audit_message.second_set_of_encrypted_key_twentyfourths[3] += 1;
    data->StoreMessage(audit_message);
    KeyDistributionAuditComplaint complaint = audit_message.GenerateComplaint(*data);

    ASSERT_THAT(complaint.VerifyPrivateReceivingKeyIsCorrect(*data), Eq(true));
}

TEST_F(ARelaysKeyDistributionAuditMessage, GeneratesAComplaintWhichDetectsIfThePrivateReceivingKeyIsIncorrect)
{
    audit_message.second_set_of_encrypted_key_twentyfourths[3] += 1;
    data->StoreMessage(audit_message);
    KeyDistributionAuditComplaint complaint = audit_message.GenerateComplaint(*data);
    complaint.private_receiving_key += 1;

    ASSERT_THAT(complaint.VerifyPrivateReceivingKeyIsCorrect(*data), Eq(false));
}

TEST_F(ARelaysKeyDistributionAuditMessage, DetectsIfTheSecretsInTheKeyDistributionMessageAreCorrect)
{
    uint160 hash_of_message_containing_relay_state = HashOfMessageContainingRelayState();
    auto audit_message = relay->GenerateKeyDistributionAuditMessage(hash_of_message_containing_relay_state, *data);

    bool key_distribution_message_ok = audit_message.VerifySecretsInKeyDistributionMessageAreCorrect(*data);
    ASSERT_THAT(key_distribution_message_ok, Eq(true));
}

void ARelaysKeyDistributionAuditMessage::CorruptKeyDistributionMessage(uint64_t position)
{
    KeyDistributionMessage bad_key_distribution_message = data->GetMessage(relay->key_quarter_locations[0].message_hash);
    bad_key_distribution_message.encrypted_key_quarters[position / 6].encrypted_key_twentyfourths[position % 6] += 1;
    data->StoreMessage(bad_key_distribution_message);
    for (auto &location : relay->key_quarter_locations)
        location.message_hash = bad_key_distribution_message.GetHash160();
}

class ARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage :
        public ARelaysKeyDistributionAuditMessage
{
public:
    KeyDistributionAuditMessage audit_message;

    virtual void SetUp()
    {
        ARelaysKeyDistributionAuditMessage::SetUp();
        DoCachedSetUpForARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage();
    }

    void DoCachedSetUpForARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage()
    {
        DECLARE_CACHED_DATA();
        DECLARE_CACHED_VARIABLE(KeyDistributionAuditMessage, audit_message);

        if (FIRST_SETUP())
        {
            DoSetUpForARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage();
            SAVE_CACHED_DATA();
            SAVE_CACHED_VARIABLE(audit_message);
        }
        else
        {
            LOAD_CACHED_DATA();
            LOAD_CACHED_VARIABLE(audit_message);
        }
    }

    void DoSetUpForARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage()
    {
        CorruptKeyDistributionMessage(2);

        uint160 hash_of_message_containing_relay_state = HashOfMessageContainingRelayState();
        audit_message = relay->GenerateKeyDistributionAuditMessage(hash_of_message_containing_relay_state, *data);
        data->StoreMessage(audit_message);
    }

    virtual void TearDown()
    {
        ARelaysKeyDistributionAuditMessage::TearDown();
    }
};

TEST_F(ARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage,
       DetectsThatTheSecretsInTheKeyDistributionMessageAreIncorrect)
{
    bool key_distribution_message_ok = audit_message.VerifySecretsInKeyDistributionMessageAreCorrect(*data);
    ASSERT_THAT(key_distribution_message_ok, Eq(false));
}

TEST_F(ARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage,
       GeneratesAKeyDistributionAuditFailureMessage)
{
    auto failure_message = audit_message.GenerateAuditFailureMessage(*data);
    ASSERT_THAT(failure_message.position_of_bad_secret, Eq(2));
}

TEST_F(ARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage,
       GeneratesAFailureMessageWhichVerifiesThatTheEnclosedKeyTwentyFourthIsCorrect)
{
    auto failure_message = audit_message.GenerateAuditFailureMessage(*data);
    bool correct_secret_enclosed = failure_message.VerifyEnclosedPrivateKeyTwentyFourthIsCorrect(*data);
    ASSERT_THAT(correct_secret_enclosed, Eq(true));
}

TEST_F(ARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage,
       GeneratesAFailureMessageWhichDetectsIfTheEnclosedKeyTwentyFourthIsIncorrect)
{
    auto failure_message = audit_message.GenerateAuditFailureMessage(*data);
    failure_message.private_key_twentyfourth += 1;
    bool correct_secret_enclosed = failure_message.VerifyEnclosedPrivateKeyTwentyFourthIsCorrect(*data);
    ASSERT_THAT(correct_secret_enclosed, Eq(false));
}

TEST_F(ARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage,
       GeneratesAFailureMessageWhichVerifiesThatTheSecretInTheKeyDistributionMessageWasBad)
{
    auto failure_message = audit_message.GenerateAuditFailureMessage(*data);
    bool distribution_message_was_bad = failure_message.VerifyKeyTwentyFourthInKeyDistributionMessageIsIncorrect(*data);
    ASSERT_THAT(distribution_message_was_bad, Eq(true));
}

TEST_F(ARelaysKeyDistributionAuditMessageWhichRevealsABadSecretInTheDistributionMessage,
       GeneratesAFailureMessageWhichDetectsIfTheSecretInTheKeyDistributionMessageWasNotBad)
{
    auto failure_message = audit_message.GenerateAuditFailureMessage(*data);
    failure_message.key_distribution_message_hash = key_distribution_message.GetHash160(); // uncorrupted message
    bool distribution_message_was_bad = failure_message.VerifyKeyTwentyFourthInKeyDistributionMessageIsIncorrect(*data);
    ASSERT_THAT(distribution_message_was_bad, Eq(false));
}

TEST_F(ARelayInARelayStateWhichHasProcessedItsKeyDistributionMessage,
       GeneratesAGoodbyeMessageWithAValidSignature)
{
    GoodbyeMessage goodbye = relay->GenerateGoodbyeMessage(*data);
    goodbye.Sign(*data);
    ASSERT_TRUE(goodbye.VerifySignature(*data));
}
