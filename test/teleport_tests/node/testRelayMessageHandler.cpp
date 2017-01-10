#include <src/base/util_hex.h>
#include "gmock/gmock.h"
#include "TestPeer.h"
#include "RelayMessageHandler.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


#define TEST_START_TIME 1000000000

class ARelayMessageHandler : public Test
{
public:
    MemoryDataStore msgdata, creditdata, keydata;
    CreditSystem *credit_system;
    RelayMessageHandler *relay_message_handler;
    Calendar calendar;
    TestPeer peer;
    Data *data;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        relay_message_handler = new RelayMessageHandler(Data(msgdata, creditdata, keydata));
        relay_message_handler->SetCreditSystem(credit_system);
        relay_message_handler->SetCalendar(&calendar);

        relay_message_handler->SetNetwork(peer.network);

        data = &relay_message_handler->data;

        SetLatestMinedCreditMessageBatchNumberTo(1);

        relay_message_handler->AddScheduledTasks();
        relay_message_handler->scheduler.running = false;
    }

    virtual void TearDown()
    {
        delete relay_message_handler;
        delete credit_system;
    }

    RelayJoinMessage ARelayJoinMessageWithAValidSignature();

    MinedCreditMessage AMinedCreditMessageWithNonZeroTotalWork();

    void CorruptStoredPublicKeyOfMinedCreditMessage(uint160 mined_credit_message_hash);

    void SetLatestMinedCreditMessageBatchNumberTo(uint32_t batch_number)
    {
        MinedCreditMessage latest_msg;
        latest_msg.mined_credit.network_state.batch_number = batch_number;

        data->StoreMessage(latest_msg);
        relay_message_handler->relay_state.latest_mined_credit_message_hash = latest_msg.GetHash160();
    }

    template <typename T>
    CDataStream GetDataStream(T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << std::string("relay") << message.Type() << message;
        return ss;
    }
};

TEST_F(ARelayMessageHandler, StartsWithNoRelaysInItsState)
{
    RelayState relay_state = relay_message_handler->relay_state;
    ASSERT_THAT(relay_state.relays.size(), Eq(0));
}

MinedCreditMessage ARelayMessageHandler::AMinedCreditMessageWithNonZeroTotalWork()
{
    MinedCreditMessage msg;
    msg.mined_credit.network_state.difficulty = 10;
    msg.mined_credit.network_state.batch_number = 1;
    msg.mined_credit.keydata = Point(SECP256K1, 5).getvch();
    keydata[Point(SECP256K1, 5)]["privkey"] = CBigNum(5);
    return msg;
}

TEST_F(ARelayMessageHandler, DetectsWhetherTheMinedCreditMessageHashInARelayJoinMessageIsInTheMainChain)
{
    RelayJoinMessage relay_join_message;
    auto msg = AMinedCreditMessageWithNonZeroTotalWork();

    relay_join_message.mined_credit_message_hash = msg.GetHash160();
    bool in_main_chain = relay_message_handler->MinedCreditMessageHashIsInMainChain(relay_join_message);
    ASSERT_THAT(in_main_chain, Eq(false));

    credit_system->AddToMainChain(msg);
    in_main_chain = relay_message_handler->MinedCreditMessageHashIsInMainChain(relay_join_message);
    ASSERT_THAT(in_main_chain, Eq(true));
}


RelayJoinMessage ARelayMessageHandler::ARelayJoinMessageWithAValidSignature()
{
    auto msg = AMinedCreditMessageWithNonZeroTotalWork();
    credit_system->StoreMinedCreditMessage(msg);
    credit_system->AddToMainChain(msg);
    RelayJoinMessage relay_join_message;
    relay_join_message.mined_credit_message_hash = msg.GetHash160();
    relay_join_message.PopulatePublicKeySet(keydata);
    relay_join_message.PopulatePrivateKeySixteenths(keydata);
    relay_join_message.Sign(*data);
    return relay_join_message;
}

void ARelayMessageHandler::CorruptStoredPublicKeyOfMinedCreditMessage(uint160 mined_credit_message_hash)
{
    MinedCreditMessage msg = msgdata[mined_credit_message_hash]["msg"];
    Point public_key;
    public_key.setvch(msg.mined_credit.keydata);
    msg.mined_credit.keydata = (public_key + Point(SECP256K1, 1)).getvch();
    msgdata[mined_credit_message_hash]["msg"] = msg;
}

TEST_F(ARelayMessageHandler, UsesTheMinedCreditMessagePublicKeyToCheckTheSignatureInARelayJoinMessage)
{
    RelayJoinMessage signed_join_message = ARelayJoinMessageWithAValidSignature();
    bool ok = signed_join_message.VerifySignature(*data);
    ASSERT_THAT(ok, Eq(true));

    CorruptStoredPublicKeyOfMinedCreditMessage(signed_join_message.mined_credit_message_hash);
    ASSERT_THAT(signed_join_message.VerifySignature(*data), Eq(false));
}

TEST_F(ARelayMessageHandler, AddsARelayToTheRelayStateWhenAcceptingARelayJoinMessage)
{
    ASSERT_THAT(relay_message_handler->relay_state.relays.size(), Eq(0));

    relay_message_handler->AcceptRelayJoinMessage(ARelayJoinMessageWithAValidSignature());

    ASSERT_THAT(relay_message_handler->relay_state.relays.size(), Eq(1));
}

TEST_F(ARelayMessageHandler, RejectsARelayJoinMessageWhoseMinedCreditMessageHashHasAlreadyBeenUsed)
{
    relay_message_handler->HandleRelayJoinMessage(ARelayJoinMessageWithAValidSignature());

    auto second_join = ARelayJoinMessageWithAValidSignature();

    for (Point &public_key_part : second_join.public_key_set.key_points[0])
        public_key_part += Point(SECP256K1, 1);

    second_join.Sign(*data);

    relay_message_handler->HandleRelayJoinMessage(second_join);

    bool rejected = msgdata[second_join.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ARelayMessageHandler, RejectsARelayJoinMessageWithABadSignature)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    join_message.signature.signature += 1;
    relay_message_handler->HandleRelayJoinMessage(join_message);

    bool rejected = msgdata[join_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ARelayMessageHandler, AcceptsAValidRelayJoinMessageWithAValidSignature)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    relay_message_handler->HandleRelayJoinMessage(join_message);

    bool rejected = msgdata[join_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(false));
}

TEST_F(ARelayMessageHandler,
       RejectsARelayJoinMessageWhoseMinedCreditIsMoreThanThreeBatchesOlderThanTheLatest)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    MinedCreditMessage relays_message = msgdata[join_message.mined_credit_message_hash]["msg"];

    SetLatestMinedCreditMessageBatchNumberTo(relays_message.mined_credit.network_state.batch_number + 4);

    relay_message_handler->HandleRelayJoinMessage(join_message);

    bool rejected = msgdata[join_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ARelayMessageHandler,
       RejectsARelayJoinMessageWhoseMinedCreditIsNewerThanTheLatest)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    MinedCreditMessage relays_message = msgdata[join_message.mined_credit_message_hash]["msg"];

    SetLatestMinedCreditMessageBatchNumberTo(0);

    relay_message_handler->HandleRelayJoinMessage(join_message);

    bool rejected = msgdata[join_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}


class ARelayMessageHandlerWhichHasAccepted37Relays : public ARelayMessageHandler
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandler::SetUp();
        static RelayState reference_test_relay_state;
        static MemoryDataStore reference_keydata, reference_creditdata, reference_msgdata;

        if (reference_test_relay_state.relays.size() == 0)
        {
            DoFirstSetUp();
            reference_test_relay_state = relay_message_handler->relay_state;
            reference_keydata = keydata;
            reference_msgdata = msgdata;
            reference_creditdata = creditdata;
        }
        else
        {
            relay_message_handler->relay_state = reference_test_relay_state;
            keydata = reference_keydata;
            msgdata = reference_msgdata;
            creditdata = reference_creditdata;
        }
    }

    void DoFirstSetUp()
    {
        for (uint32_t i = 1; i <= 37; i++)
            AddARelay();
    }

    void AddARelay()
    {
        AddAMinedCreditMessage();
        relay_message_handler->relay_state.latest_mined_credit_message_hash = calendar.LastMinedCreditMessageHash();
        RelayJoinMessage join_message = Relay().GenerateJoinMessage(keydata, calendar.LastMinedCreditMessageHash());
        join_message.Sign(*data);
        data->StoreMessage(join_message);
        relay_message_handler->HandleRelayJoinMessage(join_message);
    }

    void AddAMinedCreditMessage()
    {
        MinedCreditMessage last_msg = calendar.LastMinedCreditMessage();
        MinedCreditMessage msg;
        msg.mined_credit.network_state = credit_system->SucceedingNetworkState(last_msg);
        msg.mined_credit.keydata = Point(SECP256K1, 2).getvch();
        keydata[Point(SECP256K1, 2)]["privkey"] = CBigNum(2);
        uint160 msg_hash = msg.GetHash160();
        credit_system->StoreMinedCreditMessage(msg);
        if (msg.mined_credit.network_state.batch_number % 5 == 0)
            creditdata[msg_hash]["is_calend"] = true;
        calendar.AddToTip(msg, credit_system);
        credit_system->AddToMainChain(msg);
    }

    virtual void TearDown()
    {
        SetMockTimeMicros(0);
        ARelayMessageHandler::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasAccepted37Relays, Has37Relays)
{
    ASSERT_THAT(relay_message_handler->relay_state.relays.size(), Eq(37));
}

class ARelayMessageHandlerWithAKeyDistributionMessage : public ARelayMessageHandlerWhichHasAccepted37Relays
{
public:
    KeyDistributionMessage key_distribution_message;
    Relay *relay;

    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasAccepted37Relays::SetUp();

        relay = &relay_message_handler->relay_state.relays[30];

        SetUpUsingReferenceData();

        SetMockTimeMicros(TEST_START_TIME);
    }

    void SetUpUsingReferenceData()
    {
        static KeyDistributionMessage reference_key_distribution_message;
        static RelayState reference_relay_state;
        static MemoryDataStore reference_keydata, reference_msgdata, reference_creditdata;

        if (reference_key_distribution_message.relay_number == 0)
        {
            key_distribution_message = relay->GenerateKeyDistributionMessage(*data, relay->mined_credit_message_hash,
                                                                             relay_message_handler->relay_state);
            data->StoreMessage(key_distribution_message);

            EnsureChosenRelayIsAKeyQuarterHolder();

            reference_key_distribution_message = key_distribution_message;
            reference_relay_state = relay_message_handler->relay_state;
            reference_keydata = keydata;
            reference_msgdata = msgdata;
            reference_creditdata = creditdata;
        }
        else
        {
            key_distribution_message = reference_key_distribution_message;
            keydata = reference_keydata;
            msgdata = reference_msgdata;
            creditdata = reference_creditdata;
            relay_message_handler->relay_state = reference_relay_state;
        }
        relay = &relay_message_handler->relay_state.relays[30];
    }

    void EnsureChosenRelayIsAKeyQuarterHolder()
    {
        while (not ChosenRelayIsAKeyQuarterHolder())
        {
            AddARelay();
            relay = &relay_message_handler->relay_state.relays[30];
            AssignQuarterHoldersToRemainingRelays();
        }
    }

    bool ChosenRelayIsAKeyQuarterHolder()
    {
        return NumberOfARelayWhoSharedAQuarterKeyWithChosenRelay() != 0;
    }

    uint64_t NumberOfARelayWhoSharedAQuarterKeyWithChosenRelay()
    {
        for (auto &existing_relay : relay_message_handler->relay_state.relays)
            if (VectorContainsEntry(existing_relay.key_quarter_holders, relay->number))
                return existing_relay.number;
        return 0;
    }

    void AssignQuarterHoldersToRemainingRelays()
    {
        uint160 hash = 1;
        for (auto &existing_relay : relay_message_handler->relay_state.relays)
            if (existing_relay.key_quarter_holders.size() == 0)
                relay_message_handler->relay_state.AssignKeyPartHoldersToRelay(existing_relay, hash++);
    }


    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasAccepted37Relays::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage, RejectsAKeyDistributionMessageWithABadSignature)
{
    key_distribution_message.signature.signature += 1;
    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);

    bool rejected = msgdata[key_distribution_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage, RejectsAKeyDistributionMessageWithTheWrongNumbersOfSecrets)
{
    key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders.push_back(1);

    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);

    bool rejected = msgdata[key_distribution_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       SchedulesATaskToEncodeADurationWithoutResponseAfterReceivingAValidKeyDistributionMessage)
{
    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);

    ASSERT_TRUE(relay_message_handler->scheduler.TaskIsScheduledForTime("key_distribution",
                                                                        key_distribution_message.GetHash160(),
                                                                        TEST_START_TIME + RESPONSE_WAIT_TIME));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       SendsAKeyDistributionComplaintInResponseToABadEncryptedKeyQuarter)
{
    key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders[1] += 1;
    key_distribution_message.Sign(*data);

    relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);

    std::vector<uint160> complaints = msgdata[key_distribution_message.GetHash160()]["complaints"];

    ASSERT_THAT(complaints.size(), Eq(1));
    KeyDistributionComplaint complaint = data->GetMessage(complaints[0]);
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "key_distribution_complaint", complaint));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       SendsAKeyDistributionComplaintInResponseToABadEncryptedKeySixteenth)
{
    key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[1] += 1;
    key_distribution_message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[1] += 1;
    key_distribution_message.Sign(*data);

    relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);

    std::vector<uint160> complaints = msgdata[key_distribution_message.GetHash160()]["complaints"];

    ASSERT_THAT(complaints.size(), Eq(2));
    KeyDistributionComplaint complaint1 = data->GetMessage(complaints[0]);
    KeyDistributionComplaint complaint2 = data->GetMessage(complaints[1]);
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "key_distribution_complaint", complaint1));
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "key_distribution_complaint", complaint2));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       SendsAKeyDistributionComplaintInResponseToABadPointInThePublicKeySet)
{
    key_distribution_message.Sign(*data);

    relay->public_key_set.key_points[1][2] += Point(CBigNum(1));
    relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);

    std::vector<uint160> complaints = msgdata[key_distribution_message.GetHash160()]["complaints"];

    ASSERT_THAT(complaints.size(), Eq(3));
    KeyDistributionComplaint complaint1 = data->GetMessage(complaints[0]);
    KeyDistributionComplaint complaint2 = data->GetMessage(complaints[1]);
    KeyDistributionComplaint complaint3 = data->GetMessage(complaints[2]);
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "key_distribution_complaint", complaint1));
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "key_distribution_complaint", complaint2));
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "key_distribution_complaint", complaint3));
}

class ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage :
        public ARelayMessageHandlerWithAKeyDistributionMessage
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::SetUp();

        static RelayState reference_relay_state;
        static MemoryDataStore reference_keydata, reference_msgdata, reference_creditdata;

        if (reference_relay_state.relays.size() == 0)
        {
            relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);

            reference_relay_state = relay_message_handler->relay_state;
            reference_keydata = keydata;
            reference_msgdata = msgdata;
            reference_creditdata = creditdata;
        }
        else
        {
            keydata = reference_keydata;
            msgdata = reference_msgdata;
            creditdata = reference_creditdata;
            relay_message_handler->relay_state = reference_relay_state;
        }

    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage, RejectsAKeyDistributionComplaintWithABadSignature)
{
    KeyDistributionComplaint complaint(key_distribution_message.GetHash160(), 1, 1, *data);
    complaint.Sign(*data);
    complaint.signature.signature += 1;
    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);

    bool rejected = msgdata[complaint.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage, RejectsAKeyDistributionComplaintAboutAGoodSecret)
{
    KeyDistributionComplaint complaint(key_distribution_message.GetHash160(), 1, 1, *data);
    complaint.Sign(*data);

    bool complaint_is_valid = complaint.IsValid(*data);
    ASSERT_THAT(complaint_is_valid, Eq(false));

    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);

    bool rejected = msgdata[complaint.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}


class ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessageWithABadSecret :
        public ARelayMessageHandlerWithAKeyDistributionMessage
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::SetUp();
        key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders[1] += 1;
        key_distribution_message.Sign(*data);
        data->StoreMessage(key_distribution_message);
        relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessageWithABadSecret,
       AcceptsAKeyDistributionComplaintAboutTheBadSecret)
{
    KeyDistributionComplaint complaint(key_distribution_message.GetHash160(), KEY_DISTRIBUTION_COMPLAINT_KEY_QUARTERS,
                                       1, *data);
    complaint.Sign(*data);

    bool complaint_is_valid = complaint.IsValid(*data);
    ASSERT_THAT(complaint_is_valid, Eq(true));

    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);

    bool rejected = msgdata[complaint.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(false));
}


class ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet :
        public ARelayMessageHandlerWithAKeyDistributionMessage
{
public:
    KeyDistributionComplaint complaint;

    virtual void SetUp()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::SetUp();
        static KeyDistributionComplaint reference_complaint;
        static RelayState reference_relay_state;
        static MemoryDataStore reference_keydata, reference_msgdata, reference_creditdata;

        if (reference_relay_state.relays.size() == 0)
        {
            DoSetUp();

            reference_relay_state = relay_message_handler->relay_state;
            reference_keydata = keydata;
            reference_msgdata = msgdata;
            reference_creditdata = creditdata;
            reference_complaint = complaint;
        }
        else
        {
            keydata = reference_keydata;
            msgdata = reference_msgdata;
            creditdata = reference_creditdata;
            relay_message_handler->relay_state = reference_relay_state;
            complaint = reference_complaint;
        }
    }

    void DoSetUp()
    {
        EnsureThereIsSomethingToComplainAboutAndThatTheChosenRelayIsHoldingSecrets();

        relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);
        complaint = KeyDistributionComplaint(key_distribution_message.GetHash160(), 0, 1, *data);
        complaint.Sign(*data);
        data->StoreMessage(complaint);
    }

    void EnsureThereIsSomethingToComplainAboutAndThatTheChosenRelayIsHoldingSecrets()
    {
        ChangeTheChosenRelaysPublicKeySetAndKeepTheNewKeys();
        SendAKeyDistributionMessageWhichGivesAQuarterKeyToTheChosenRelay();
    }

    void ChangeTheChosenRelaysPublicKeySetAndKeepTheNewKeys()
    {
        CBigNum privkey = keydata[relay->public_key_set.key_points[1][1]]["privkey"];
        relay->public_key_set.key_points[1][1] += Point(CBigNum(1));
        keydata[relay->public_key_set.key_points[1][1]]["privkey"] = privkey + 1;
    }

    void SendAKeyDistributionMessageWhichGivesAQuarterKeyToTheChosenRelay()
    {
        uint64_t key_sharer_number = NumberOfARelayWhoSharedAQuarterKeyWithChosenRelay();
        auto key_sharer = relay_message_handler->relay_state.GetRelayByNumber(key_sharer_number);
        auto message = key_sharer->GenerateKeyDistributionMessage(*data, key_sharer->mined_credit_message_hash,
                                                                  relay_message_handler->relay_state);
        data->StoreMessage(message);
        relay_message_handler->HandleKeyDistributionMessage(message);
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet,
       AcceptsAKeyDistributionComplaintAboutTheBadPoint)
{
    bool complaint_is_valid = complaint.IsValid(*data);
    ASSERT_THAT(complaint_is_valid, Eq(true));

    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);

    bool rejected = msgdata[complaint.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(false));
}

TEST_F(ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet,
       AddsAnObituaryForTheOffendingRelayWhenProcessingAValidKeyDistributionComplaint)
{
    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);

    ASSERT_THAT(relay->obituary_hash, Ne(0));
}


class ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint : 
   public ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::SetUp();

        relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
    }
    
    virtual void TearDown()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint,
       SendsSecretRecoveryMessagesFromEachQuarterHolderWhosePrivateSigningKeyIsKnown)
{
    std::vector<uint160> secret_recovery_message_hashes = msgdata[relay->obituary_hash]["secret_recovery_messages"];
    ASSERT_THAT(secret_recovery_message_hashes.size(), Eq(4));

    SecretRecoveryMessage secret_recovery_message;
    for (auto secret_recovery_message_hash : secret_recovery_message_hashes)
    {
        secret_recovery_message = data->GetMessage(secret_recovery_message_hash);
        ASSERT_TRUE(VectorContainsEntry(relay->key_quarter_holders, secret_recovery_message.quarter_holder_number));
        ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "secret_recovery", secret_recovery_message));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint,
       SchedulesATaskToCheckWhichQuarterHoldersHaveRespondedToTheObituary)
{
    ASSERT_TRUE(relay_message_handler->scheduler.TaskIsScheduledForTime("obituary",
                                                                        relay->obituary_hash,
                                                                        TEST_START_TIME + RESPONSE_WAIT_TIME));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint,
       GeneratesObituariesForKeyQuarterHoldersWhoHaveNotRespondedToTheObituaryWhenCompletingTheScheduledTask)
{
    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    msgdata[relay->obituary_hash]["secret_recovery_messages"] = std::vector<uint160>();
    relay_message_handler->scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (auto quarter_holder_number : relay->key_quarter_holders)
    {
        auto quarter_holder = relay_message_handler->relay_state.GetRelayByNumber(quarter_holder_number);
        ASSERT_THAT(quarter_holder->obituary_hash, Ne(0));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint,
       DoesntGeneratesObituariesForKeyQuarterHoldersWhoHaveRespondedToTheObituaryWhenCompletingTheScheduledTask)
{
    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    relay_message_handler->scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (auto quarter_holder_number : relay->key_quarter_holders)
    {
        auto quarter_holder = relay_message_handler->relay_state.GetRelayByNumber(quarter_holder_number);
        ASSERT_THAT(quarter_holder->obituary_hash, Eq(0));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint,
       HandlesTheSecretRecoveryMessagesWhichItGeneratesInResponseToTheObituary)
{
    std::vector<uint160> secret_recovery_message_hashes = msgdata[relay->obituary_hash]["secret_recovery_messages"];
    for (auto secret_recovery_message_hash : secret_recovery_message_hashes)
    {
        bool handled = msgdata[secret_recovery_message_hash]["handled"];
        ASSERT_THAT(handled, Eq(true));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint,
       SchedulesTasksToCheckForComplaintsWhenHandlingASecretRecoveryMessage)
{
    std::vector<uint160> secret_recovery_message_hashes = msgdata[relay->obituary_hash]["secret_recovery_messages"];
    for (auto secret_recovery_message_hash : secret_recovery_message_hashes)
    {
        ASSERT_TRUE(relay_message_handler->scheduler.TaskIsScheduledForTime("secret_recovery",
                                                                            secret_recovery_message_hash,
                                                                            TEST_START_TIME + RESPONSE_WAIT_TIME));
    }
}


class ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages :
        public ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint
{
public:
    uint160 last_secret_recovery_message_hash;

    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint::SetUp();

        UnprocessLastSecretRecoveryMessage();
        ForgetKeyQuartersHeldByDeadRelay();
    }


    void UnprocessLastSecretRecoveryMessage()
    {
        std::vector<uint160> secret_recovery_message_hashes = msgdata[relay->obituary_hash]["secret_recovery_messages"];
        last_secret_recovery_message_hash = secret_recovery_message_hashes.back();
        secret_recovery_message_hashes.pop_back();
        msgdata[relay->obituary_hash]["secret_recovery_messages"] = secret_recovery_message_hashes;
        msgdata[last_secret_recovery_message_hash]["handled"] = false;
    }

    void ForgetKeyQuartersHeldByDeadRelay()
    {
        for (auto &existing_relay : relay_message_handler->relay_state.relays)
            if (VectorContainsEntry(existing_relay.key_quarter_holders, relay->number))
                ForgetKeyQuartersOfGivenRelayHeldByDeadRelay(existing_relay);
    }

    void ForgetKeyQuartersOfGivenRelayHeldByDeadRelay(Relay &given_relay)
    {
        auto key_quarter_position = PositionOfEntryInVector(relay->number, given_relay.key_quarter_holders);

        for (auto key_sixteenth_position = key_quarter_position * 4;
             key_sixteenth_position < 4 + key_quarter_position * 4;
             key_sixteenth_position++)
        {
            keydata.Delete(given_relay.PublicKeySixteenths()[key_sixteenth_position]);
        }
    }

    bool KeyQuartersHeldByDeadRelayAreKnown()
    {
        bool known = true;
        for (auto &existing_relay : relay_message_handler->relay_state.relays)
            if (VectorContainsEntry(existing_relay.key_quarter_holders, relay->number))
                if (not KeyQuartersOfGivenRelayHeldByDeadRelayAreKnown(existing_relay))
                    known = false;
        return known;
    }

    bool KeyQuartersOfGivenRelayHeldByDeadRelayAreKnown(Relay &given_relay)
    {
        auto key_quarter_position = PositionOfEntryInVector(relay->number, given_relay.key_quarter_holders);
        for (auto key_sixteenth_position = key_quarter_position * 4; key_sixteenth_position < key_quarter_position * 5;
                                                                     key_sixteenth_position++)
            if (not keydata[given_relay.PublicKeySixteenths()[key_sixteenth_position]].HasProperty("privkey"))
                return false;
        return true;
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaint::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       ReconstructsKeyPartsHeldByTheDeadRelayIfTheSuccessorsPrivateSigningKeyIsAvailableWhenProcessingTheFourth)
{
    ASSERT_FALSE(KeyQuartersHeldByDeadRelayAreKnown());

    SecretRecoveryMessage last_secret_recovery_message = data->GetMessage(last_secret_recovery_message_hash);
    relay_message_handler->Handle(last_secret_recovery_message, NULL);

    ASSERT_TRUE(KeyQuartersHeldByDeadRelayAreKnown());
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
        SendsASecretRecoveryComplaintIfASecretRecoveryMessageContainsASecretWhichFailsDecryption)
{
    SecretRecoveryMessage last_secret_recovery_message = data->GetMessage(last_secret_recovery_message_hash);
    last_secret_recovery_message.quartets_of_encrypted_shared_secret_quarters[0][1] += 1;

    relay_message_handler->Handle(last_secret_recovery_message, NULL);

    std::vector<uint160> complaints = msgdata[last_secret_recovery_message.GetHash160()]["complaints"];
    ASSERT_THAT(complaints.size(), Eq(1));

    SecretRecoveryComplaint complaint = data->GetMessage(complaints[0]);
    ASSERT_THAT(complaint.position_of_key_sharer, Eq(0));
    ASSERT_THAT(complaint.position_of_bad_encrypted_secret, Eq(1));
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       GeneratesAnObituaryForTheBadKeyQuarterHolderInResponseToAValidSecretRecoveryComplaint)
{
    SecretRecoveryMessage last_secret_recovery_message = data->GetMessage(last_secret_recovery_message_hash);
    last_secret_recovery_message.quartets_of_encrypted_shared_secret_quarters[0][1] += 1;

    relay_message_handler->Handle(last_secret_recovery_message, NULL);
    auto key_quarter_holder = last_secret_recovery_message.GetKeyQuarterHolder(*data);
    ASSERT_THAT(key_quarter_holder->obituary_hash, Ne(0));
}

class ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter :
        public ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages
{
public:
    SecretRecoveryMessage bad_recovery_message;

    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages::SetUp();

        bad_recovery_message = GenerateRecoveryMessageWithBadSharedSecretQuarter();
        data->StoreMessage(bad_recovery_message);

        relay_message_handler->Handle(bad_recovery_message, &peer);
    }

    SecretRecoveryMessage GenerateRecoveryMessageWithBadSharedSecretQuarter()
    {
        SecretRecoveryMessage recovery_message = data->GetMessage(last_secret_recovery_message_hash);

        auto successor = recovery_message.GetSuccessor(*data);

        Point bad_shared_secret_quarter(CBigNum(12345));
        PopulateRecoveryMessageWithABadSharedSecretQuarter(recovery_message, successor, bad_shared_secret_quarter);

        return recovery_message;
    }

    void PopulateRecoveryMessageWithABadSharedSecretQuarter(SecretRecoveryMessage &recovery_message,
                                                            Relay *successor, Point bad_shared_secret_quarter)
    {
        CBigNum encoded_bad_shared_secret_quarter = StorePointInBigNum(bad_shared_secret_quarter);
        CBigNum encrypted_bad_shared_secret_quarter = successor->EncryptSecret(encoded_bad_shared_secret_quarter);
        Point corresponding_point(encoded_bad_shared_secret_quarter);

        recovery_message.quartets_of_encrypted_shared_secret_quarters[0][2] = encrypted_bad_shared_secret_quarter;
        recovery_message.quartets_of_points_corresponding_to_shared_secret_quarters[0][2] = corresponding_point;
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter,
       SendsASecretRecoveryFailureMessageIfTheSuccessorsPrivateSigningKeyIsAvailable)
{
    std::vector<uint160> failure_message_hashes = msgdata[bad_recovery_message.obituary_hash]["failure_messages"];

    ASSERT_THAT(failure_message_hashes.size(), Eq(1));

    SecretRecoveryFailureMessage failure_message = data->GetMessage(failure_message_hashes[0]);

    ASSERT_THAT(failure_message.key_sharer_position, Eq(0));
    ASSERT_THAT(failure_message.shared_secret_quarter_position, Eq(2));
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", failure_message));
}