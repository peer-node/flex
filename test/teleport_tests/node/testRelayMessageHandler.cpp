#include <src/base/util_hex.h>
#include <test/teleport_tests/node/credit_handler/MinedCreditMessageBuilder.h>
#include "gmock/gmock.h"
#include "TestPeer.h"
#include "test/teleport_tests/node/relay_handler/RelayMessageHandler.h"

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
    RelayState *relay_state;
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
        relay_message_handler->admission_handler.scheduler.running = false;
        relay_message_handler->succession_handler.scheduler.running = false;

        relay_state = &relay_message_handler->relay_state;
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

    template <typename T>
    bool IsRejected(T message)
    {
        return msgdata[message.GetHash160()]["rejected"];
    }

    bool RelayIsDeadWithReason(uint64_t relay_number, int reason)
    {
        auto relay = relay_state->GetRelayByNumber(relay_number);
        if (relay == NULL or relay->hashes.obituary_hash == 0)
            return false;
        Obituary obituary = data->GetMessage(relay->hashes.obituary_hash);
        return obituary.reason_for_leaving == reason;
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
    bool in_main_chain = relay_message_handler->admission_handler.MinedCreditMessageHashIsInMainChain(relay_join_message);
    ASSERT_THAT(in_main_chain, Eq(false));

    credit_system->AddToMainChain(msg);
    in_main_chain = relay_message_handler->admission_handler.MinedCreditMessageHashIsInMainChain(relay_join_message);
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

    relay_message_handler->admission_handler.AcceptRelayJoinMessage(ARelayJoinMessageWithAValidSignature());

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

    ASSERT_TRUE(IsRejected(second_join));
}

TEST_F(ARelayMessageHandler, RejectsARelayJoinMessageWithTheWrongNumberOfComponents)
{
    auto join = ARelayJoinMessageWithAValidSignature();
    join.encrypted_private_key_sixteenths.pop_back();
    join.Sign(*data);
    relay_message_handler->HandleRelayJoinMessage(join);
    ASSERT_TRUE(IsRejected(join));
}

TEST_F(ARelayMessageHandler, RejectsARelayJoinMessageWithABadSignature)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    join_message.signature.signature += 1;
    relay_message_handler->HandleRelayJoinMessage(join_message);
    ASSERT_TRUE(IsRejected(join_message));
}

TEST_F(ARelayMessageHandler, AcceptsAValidRelayJoinMessageWithAValidSignature)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    relay_message_handler->HandleRelayJoinMessage(join_message);
    ASSERT_FALSE(IsRejected(join_message));
}

TEST_F(ARelayMessageHandler,
       RejectsARelayJoinMessageWhoseMinedCreditIsMoreThanThreeBatchesOlderThanTheLatest)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    MinedCreditMessage relays_message = msgdata[join_message.mined_credit_message_hash]["msg"];

    SetLatestMinedCreditMessageBatchNumberTo(relays_message.mined_credit.network_state.batch_number + 4);
    relay_message_handler->HandleRelayJoinMessage(join_message);

    ASSERT_TRUE(IsRejected(join_message));
}

TEST_F(ARelayMessageHandler,
       RejectsARelayJoinMessageWhoseMinedCreditIsNewerThanTheLatest)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    MinedCreditMessage relays_message = msgdata[join_message.mined_credit_message_hash]["msg"];

    SetLatestMinedCreditMessageBatchNumberTo(0);

    relay_message_handler->HandleRelayJoinMessage(join_message);
    ASSERT_TRUE(IsRejected(join_message));
}

TEST_F(ARelayMessageHandler, ThrowsAnExceptionIfAskedToBroadcastWhileInBlockValidationMode)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    relay_message_handler->mode = BLOCK_VALIDATION;
    EXPECT_THROW(relay_message_handler->Broadcast(join_message), std::runtime_error);
}


// macros to cache the state of the system after a time-consuming setup so that the setup
// need only be performed once for each test suite
#define DECLARE_CACHED_DATA                                                                                 \
    static MemoryDataStore reference_msgdata, reference_keydata, reference_creditdata,                      \
           reference_succession_scheduledata, reference_admission_scheduledata;                             \
    static RelayState reference_relay_state;                                                                \
    static std::vector<uint160> reference_builder_accepted_messages;

#define SAVE_CACHED_DATA                                                                                    \
    reference_msgdata = msgdata;                                                                            \
    reference_keydata = keydata;                                                                            \
    reference_creditdata = creditdata;                                                                      \
    reference_relay_state = *relay_state;                                                                   \
    reference_succession_scheduledata = relay_message_handler->succession_handler.scheduler.scheduledata;   \
    reference_admission_scheduledata = relay_message_handler->admission_handler.scheduler.scheduledata;     \
    reference_builder_accepted_messages = builder->accepted_messages;

#define LOAD_CACHED_DATA                                                                                     \
    msgdata = reference_msgdata;                                                                             \
    keydata = reference_keydata;                                                                             \
    creditdata = reference_creditdata;                                                                       \
    *relay_state = reference_relay_state;                                                                    \
    relay_message_handler->succession_handler.scheduler.scheduledata = reference_succession_scheduledata;    \
    relay_message_handler->admission_handler.scheduler.scheduledata = reference_admission_scheduledata;      \
    builder->accepted_messages = reference_builder_accepted_messages;


class ARelayMessageHandlerWithAMinedCreditMessageBuilder : public ARelayMessageHandler
{
public:
    MinedCreditMessageBuilder *builder;

    virtual void SetUp()
    {
        ARelayMessageHandler::SetUp();
        builder = new MinedCreditMessageBuilder(*data);
        relay_message_handler->SetMinedCreditMessageBuilder(builder);
    }

    virtual void TearDown()
    {
        delete builder;
        ARelayMessageHandler::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWithAMinedCreditMessageBuilder,
       SendsRelayJoinMessagesToTheMinedCreditMessageBuilderWhenTheyAreAcceptedInLiveMode)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    relay_message_handler->HandleRelayJoinMessage(join_message);
    ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, join_message.GetHash160()));
}

TEST_F(ARelayMessageHandlerWithAMinedCreditMessageBuilder,
       DoesntSendRelayJoinMessagesToTheMinedCreditMessageBuilderWhenTheyAreAcceptedInBlockValidationMode)
{
    RelayJoinMessage join_message = ARelayJoinMessageWithAValidSignature();
    relay_message_handler->mode = BLOCK_VALIDATION;
    relay_message_handler->HandleRelayJoinMessage(join_message);
    ASSERT_FALSE(VectorContainsEntry(builder->accepted_messages, join_message.GetHash160()));
}


class ARelayMessageHandlerWhichHasAccepted37Relays : public ARelayMessageHandlerWithAMinedCreditMessageBuilder
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAMinedCreditMessageBuilder::SetUp();
        DECLARE_CACHED_DATA;

        if (reference_relay_state.relays.size() == 0)
        {
            DoFirstSetUp();
            SAVE_CACHED_DATA;
        }
        else
        {
            LOAD_CACHED_DATA;
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
        ARelayMessageHandlerWithAMinedCreditMessageBuilder::TearDown();
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
        DECLARE_CACHED_DATA;

        if (reference_key_distribution_message.relay_number == 0)
        {
            key_distribution_message = relay->GenerateKeyDistributionMessage(*data, relay->hashes.mined_credit_message_hash,
                                                                             relay_message_handler->relay_state);
            data->StoreMessage(key_distribution_message);

            EnsureChosenRelayIsAKeyQuarterHolder();

            SAVE_CACHED_DATA;
            reference_key_distribution_message = key_distribution_message;
        }
        else
        {
            key_distribution_message = reference_key_distribution_message;
            LOAD_CACHED_DATA;
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
            if (VectorContainsEntry(existing_relay.holders.key_quarter_holders, relay->number))
                return existing_relay.number;
        return 0;
    }

    Relay *GetRelayWhoSharedAQuarterKeyWithChosenRelay()
    {
        return relay_state->GetRelayByNumber(NumberOfARelayWhoSharedAQuarterKeyWithChosenRelay());
    }

    void SendAKeyDistributionMessageWhichGivesAQuarterKeyToTheChosenRelay()
    {
        uint64_t key_sharer_number = NumberOfARelayWhoSharedAQuarterKeyWithChosenRelay();
        auto key_sharer = relay_message_handler->relay_state.GetRelayByNumber(key_sharer_number);
        auto message = key_sharer->GenerateKeyDistributionMessage(*data, key_sharer->hashes.mined_credit_message_hash,
                                                                  relay_message_handler->relay_state);
        data->StoreMessage(message);
        relay_message_handler->HandleKeyDistributionMessage(message);
    }

    void AssignQuarterHoldersToRemainingRelays()
    {
        uint160 hash = 1;
        for (auto &existing_relay : relay_message_handler->relay_state.relays)
            if (existing_relay.holders.key_quarter_holders.size() == 0)
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
    ASSERT_TRUE(IsRejected(key_distribution_message));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage, RejectsAKeyDistributionMessageWithTheWrongRelayNumber)
{
    key_distribution_message.relay_number += 1;
    key_distribution_message.Sign(*data);

    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);

    ASSERT_TRUE(IsRejected(key_distribution_message));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage, RejectsAKeyDistributionMessageWithTheWrongNumbersOfSecrets)
{
    key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders.push_back(1);
    key_distribution_message.Sign(*data);

    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);

    ASSERT_TRUE(IsRejected(key_distribution_message));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       SchedulesATaskToEncodeADurationWithoutResponseAfterReceivingAValidKeyDistributionMessage)
{
    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);

    ASSERT_TRUE(relay_message_handler->admission_handler.scheduler.TaskIsScheduledForTime(
                "key_distribution", key_distribution_message.GetHash160(), TEST_START_TIME + RESPONSE_WAIT_TIME));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       EncodesAValidKeyDistributionMessage)
{
    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);
    ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, key_distribution_message.GetHash160()));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       DoesntScheduleATaskAfterReceivingAValidKeyDistributionMessageInBlockValidationMode)
{
    relay_message_handler->mode = BLOCK_VALIDATION;
    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);

    ASSERT_FALSE(relay_message_handler->admission_handler.scheduler.TaskIsScheduledForTime(
                 "key_distribution", key_distribution_message.GetHash160(), TEST_START_TIME + RESPONSE_WAIT_TIME));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       SendsAKeyDistributionComplaintInResponseToABadEncryptedKeyQuarter)
{
    relay_state->remove_dead_relays = false;
    key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders[1] += 1;
    key_distribution_message.Sign(*data);

    relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);

    vector<uint160> complaints = relay->hashes.key_distribution_complaint_hashes;

    ASSERT_THAT(complaints.size(), Eq(1));
    KeyDistributionComplaint complaint = data->GetMessage(complaints[0]);
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "key_distribution_complaint", complaint));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       SendsASingleKeyDistributionComplaintInResponseToBadEncryptedKeySixteenths)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders[1] += 1;
    key_distribution_message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders[1] += 1;
    key_distribution_message.Sign(*data);

    relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);

    vector<uint160> complaints = relay->hashes.key_distribution_complaint_hashes;

    ASSERT_THAT(complaints.size(), Eq(1));
    KeyDistributionComplaint complaint = data->GetMessage(complaints[0]);
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "key_distribution_complaint", complaint));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       SendsAKeyDistributionComplaintInResponseToABadPointInThePublicKeySet)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    key_distribution_message.Sign(*data);
    relay->public_key_set.key_points[1][2] += Point(CBigNum(1));
    relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);

    vector<uint160> complaints = relay->hashes.key_distribution_complaint_hashes;

    ASSERT_THAT(complaints.size(), Eq(1));
    KeyDistributionComplaint complaint = data->GetMessage(complaints[0]);
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "key_distribution_complaint", complaint));
}

TEST_F(ARelayMessageHandlerWithAKeyDistributionMessage,
       DoesntSendAKeyDistributionComplaintInBlockValidationMode)
{
    relay_message_handler->mode = BLOCK_VALIDATION;
    key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders[1] += 1;
    key_distribution_message.Sign(*data);
    relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);

    ASSERT_THAT(relay->hashes.key_distribution_complaint_hashes.size(), Eq(0));
}


class ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage :
        public ARelayMessageHandlerWithAKeyDistributionMessage
{
public:
    Relay *key_sharer;

    virtual void SetUp()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::SetUp();

        DECLARE_CACHED_DATA;

        if (reference_relay_state.relays.size() == 0)
        {
            relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);

            SAVE_CACHED_DATA;
        }
        else
        {
            LOAD_CACHED_DATA;
        }
        relay = &relay_message_handler->relay_state.relays[30];
        key_sharer = GetRelayWhoSharedAQuarterKeyWithChosenRelay();
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::TearDown();
    }

    void DeletePrivateKeySixteenthsSharedWithGivenRelay()
    {
        uint32_t quarter_holder_position = PositionOfGivenRelayAsAQuarterHolder();
        for (uint32_t sixteenth_position = 4 * quarter_holder_position;
             sixteenth_position < 4 + 4 * quarter_holder_position; sixteenth_position++)
            keydata.Delete(key_sharer->PublicKeySixteenths()[sixteenth_position]);
    }

    uint32_t PositionOfGivenRelayAsAQuarterHolder()
    {
        return (uint32_t) PositionOfEntryInVector(relay->number, key_sharer->holders.key_quarter_holders);
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage, RejectsAKeyDistributionComplaintWithABadSignature)
{
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(), 1, 1, *data);
    complaint.Sign(*data);
    complaint.signature.signature += 1;
    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);

    ASSERT_TRUE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       RejectsAKeyDistributionComplaintWithAnIncorrectRecipientPrivateKey)
{
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(), 1, 1, *data);
    complaint.recipient_private_key += 1;
    complaint.Sign(*data);
    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);

    ASSERT_TRUE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage, RejectsAKeyDistributionComplaintAboutAGoodSecret)
{
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(), 1, 1, *data);
    complaint.Sign(*data);

    bool complaint_is_valid = complaint.IsValid(*data);
    ASSERT_THAT(complaint_is_valid, Eq(false));

    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);

    ASSERT_TRUE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage, MarksTheMessageAsAcceptedIfNoComplaintsAreReceived)
{
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->admission_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();
    ASSERT_THAT(relay->key_distribution_message_accepted, Eq(true));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       EncodesTheDurationWithoutResponseIfNoComplaintsAreReceived)
{
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->admission_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();
    DurationWithoutResponse duration;
    duration.message_hash = key_distribution_message.GetHash160();
    ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, duration.GetHash160()));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       DoesntMarkTheMessageAsAcceptedIfComplaintsAreReceived)
{
    relay->hashes.key_distribution_complaint_hashes = vector<uint160>{5};
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->admission_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();
    ASSERT_THAT(relay->key_distribution_message_accepted, Eq(false));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       SchedulesACheckForGoodbyeComplaintsWhenProcessingAGoodByeMessage)
{
    GoodbyeMessage goodbye_message = relay->GenerateGoodbyeMessage(*data);
    relay_message_handler->Handle(goodbye_message, &peer);
    ASSERT_THAT(relay->hashes.goodbye_message_hash, Ne(0));

    ASSERT_TRUE(relay_message_handler->succession_handler.scheduler.TaskIsScheduledForTime("goodbye",
                                                                        relay->hashes.goodbye_message_hash,
                                                                        TEST_START_TIME + RESPONSE_WAIT_TIME));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       DoesntScheduleACheckForGoodbyeComplaintsInBlockValidationMode)
{
    GoodbyeMessage goodbye_message = relay->GenerateGoodbyeMessage(*data);
    relay_message_handler->mode = BLOCK_VALIDATION;
    relay_message_handler->Handle(goodbye_message, &peer);
    ASSERT_THAT(relay->hashes.goodbye_message_hash, Ne(0));

    ASSERT_FALSE(relay_message_handler->succession_handler.scheduler.TaskIsScheduledForTime(
                 "goodbye", relay->hashes.goodbye_message_hash, TEST_START_TIME + RESPONSE_WAIT_TIME));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       RejectsAGoodbyeMessageWithTheWrongNumberOfComponents)
{
    GoodbyeMessage goodbye_message = relay->GenerateGoodbyeMessage(*data);
    goodbye_message.key_quarter_positions.push_back(0);
    goodbye_message.Sign(*data);

    relay_message_handler->Handle(goodbye_message, &peer);
    ASSERT_TRUE(IsRejected(goodbye_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       RejectsAGoodbyeMessageWithTheWrongSuccessor)
{
    GoodbyeMessage goodbye_message = relay->GenerateGoodbyeMessage(*data);
    goodbye_message.successor_number += 1;
    goodbye_message.Sign(*data);

    relay_message_handler->Handle(goodbye_message, &peer);
    ASSERT_TRUE(IsRejected(goodbye_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       RejectsAGoodbyeMessageWithTheWrongKeyQuarterSharers)
{
    GoodbyeMessage goodbye_message = relay->GenerateGoodbyeMessage(*data);
    goodbye_message.key_quarter_sharers[0] += 1;
    goodbye_message.Sign(*data);

    relay_message_handler->Handle(goodbye_message, &peer);
    ASSERT_TRUE(IsRejected(goodbye_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       RecoversKeyQuartersHeldByTheExitingRelayWhenProcessingAGoodbyeMessage)
{
    GoodbyeMessage goodbye_message = relay->GenerateGoodbyeMessage(*data);
    DeletePrivateKeySixteenthsSharedWithGivenRelay();
    relay_message_handler->Handle(goodbye_message, &peer);

    uint32_t quarter_holder_position = PositionOfGivenRelayAsAQuarterHolder();

    for (uint32_t sixteenth_position = 4 * quarter_holder_position;
         sixteenth_position < 4 + 4 * quarter_holder_position; sixteenth_position++)
    {
        auto public_key_sixteenth = key_sharer->PublicKeySixteenths()[sixteenth_position];
        ASSERT_TRUE(keydata[public_key_sixteenth].HasProperty("privkey"));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage,
       EncodesAValidGoodbyeMessageWhenProcessingIt)
{
    GoodbyeMessage goodbye_message = relay->GenerateGoodbyeMessage(*data);
    relay_message_handler->Handle(goodbye_message, &peer);
    ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, goodbye_message.GetHash160()));
}


class ARelayMessageHandlerWithAValidGoodbyeMessage :
        public ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage
{
public:
    GoodbyeMessage goodbye_message;

    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage::SetUp();
        DoSetupUsingCache();
    }

    void DoSetupUsingCache()
    {
        DECLARE_CACHED_DATA;
        static GoodbyeMessage reference_goodbye_message;
        if (reference_relay_state.relays.size() == 0)
        {
            goodbye_message = relay->GenerateGoodbyeMessage(*data);
            data->StoreMessage(goodbye_message);
            SAVE_CACHED_DATA;
            reference_goodbye_message = goodbye_message;
        }
        else
        {
            LOAD_CACHED_DATA;
            goodbye_message = reference_goodbye_message;
        }
    }

    Relay *Successor()
    {
        return relay_state->GetRelayByNumber(goodbye_message.successor_number);
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWithAValidGoodbyeMessage,
       SendsASuccessionCompletedMessageIfTheSecretsAreSuccessfullyRecovered)
{
    auto succession_completed_message = Successor()->GenerateSuccessionCompletedMessage(goodbye_message, *data);
    relay_message_handler->Handle(goodbye_message, &peer);
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "succession_completed", succession_completed_message));
}

TEST_F(ARelayMessageHandlerWithAValidGoodbyeMessage,
       DoesntSendASuccessionCompletedMessageInBlockValidationMode)
{
    relay_message_handler->mode = BLOCK_VALIDATION;
    auto succession_completed_message = Successor()->GenerateSuccessionCompletedMessage(goodbye_message, *data);
    relay_message_handler->Handle(goodbye_message, &peer);
    ASSERT_FALSE(peer.HasBeenInformedAbout("relay", "succession_completed", succession_completed_message));
}

TEST_F(ARelayMessageHandlerWithAValidGoodbyeMessage,
       RemovesTheDeadRelayWhenProcessingTheSuccessionCompletedMessage)
{
    relay_message_handler->Handle(goodbye_message, &peer);
    ASSERT_FALSE(relay_state->ContainsRelayWithNumber(goodbye_message.dead_relay_number));
}

TEST_F(ARelayMessageHandlerWithAValidGoodbyeMessage,
       RecordsTheSuccessionCompletedMessageInTheSuccessorsMessageHashData)
{
    relay_message_handler->Handle(goodbye_message, &peer);
    auto succession_completed_message = Successor()->GenerateSuccessionCompletedMessage(goodbye_message, *data);
    ASSERT_TRUE(VectorContainsEntry(Successor()->hashes.succession_completed_message_hashes,
                                    succession_completed_message.GetHash160()));
}

TEST_F(ARelayMessageHandlerWithAValidGoodbyeMessage,
       DoesntEncodeADurationWithoutResponseIfASuccessionCompletedMessageIsReceived)
{
    relay_message_handler->Handle(goodbye_message, &peer);
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();
    DurationWithoutResponse duration = GetDurationWithoutResponseAfter(goodbye_message.GetHash160());
    ASSERT_FALSE(VectorContainsEntry(builder->accepted_messages, duration.GetHash160()));
}


class ARelayMessageHandlerWhichHasProcessedAValidGoodbyeMessageAndNoSuccessionCompletedMessage :
        public ARelayMessageHandlerWithAValidGoodbyeMessage
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAValidGoodbyeMessage::SetUp();
        DoSetUpUsingCache();
    }

    void DoSetUpUsingCache()
    {
        DECLARE_CACHED_DATA;
        if (reference_relay_state.relays.size() == 0)
        {
            relay_message_handler->succession_handler.send_succession_completed_messages = false;
            relay_message_handler->Handle(goodbye_message, &peer);
            SAVE_CACHED_DATA;
        }
        else
        { LOAD_CACHED_DATA; }
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAValidGoodbyeMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeMessageAndNoSuccessionCompletedMessage,
       MarksTheSuccessorAsNotRespondingIfNoResponseIsReceived)
{
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();
    ASSERT_TRUE(RelayIsDeadWithReason(goodbye_message.successor_number, NOT_RESPONDING));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeMessageAndNoSuccessionCompletedMessage,
       EncodesTheDurationWithoutResponseIfNoResponseIsReceived)
{
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();
    DurationWithoutResponse duration;
    duration.message_hash = goodbye_message.GetHash160();
    ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, duration.GetHash160()));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeMessageAndNoSuccessionCompletedMessage,
       RejectsASuccessionCompletedMessageWithTheWrongDeadRelayNumber)
{
    auto succession_completed_message = Successor()->GenerateSuccessionCompletedMessage(goodbye_message, *data);
    succession_completed_message.dead_relay_number += 1;
    succession_completed_message.Sign(*data);
    relay_message_handler->Handle(succession_completed_message, &peer);
    ASSERT_TRUE(IsRejected(succession_completed_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeMessageAndNoSuccessionCompletedMessage,
       RejectsASuccessionCompletedMessageWithTheWrongSuccessorNumber)
{
    auto succession_completed_message = Successor()->GenerateSuccessionCompletedMessage(goodbye_message, *data);
    succession_completed_message.successor_number += 1;
    succession_completed_message.Sign(*data);
    relay_message_handler->Handle(succession_completed_message, &peer);
    ASSERT_TRUE(IsRejected(succession_completed_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeMessageAndNoSuccessionCompletedMessage,
       RejectsAGoodbyeComplaintWithAnImpossibleKeySharerPosition)
{
    GoodbyeComplaint complaint;
    complaint.goodbye_message_hash = goodbye_message.GetHash160();
    complaint.key_sharer_position = 17;
    complaint.Sign(*data);
    relay_message_handler->Handle(complaint, &peer);
    ASSERT_TRUE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeMessageAndNoSuccessionCompletedMessage,
       RejectsAGoodbyeComplaintWithAnImpossibleEncryptedSixteenthPosition)
{
    GoodbyeComplaint complaint;
    complaint.goodbye_message_hash = goodbye_message.GetHash160();
    complaint.position_of_bad_encrypted_key_sixteenth = 5;
    complaint.Sign(*data);
    relay_message_handler->Handle(complaint, &peer);
    ASSERT_TRUE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeMessageAndNoSuccessionCompletedMessage,
       RejectsAGoodbyeComplaintWithAnIncorrectPrivateReceivingKey)
{
    GoodbyeComplaint complaint;
    complaint.Populate(goodbye_message, *data);
    complaint.key_sharer_position = 0;
    complaint.position_of_bad_encrypted_key_sixteenth = 0;
    complaint.recipient_private_key = 100;
    complaint.Sign(*data);
    relay_message_handler->Handle(complaint, &peer);
    ASSERT_TRUE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeMessageAndNoSuccessionCompletedMessage,
       RejectsAGoodbyeComplaintAboutAGoodSecret)
{
    GoodbyeComplaint complaint;
    complaint.Populate(goodbye_message, *data);
    complaint.key_sharer_position = 0;
    complaint.position_of_bad_encrypted_key_sixteenth = 0;
    complaint.PopulateRecipientPrivateKey(*data);
    complaint.Sign(*data);
    relay_message_handler->Handle(complaint, &peer);
    ASSERT_TRUE(IsRejected(complaint));
}


class ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret :
        public ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage
{
public:
    GoodbyeMessage goodbye_message;
    uint160 goodbye_message_hash;

    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage::SetUp();
        DECLARE_CACHED_DATA;
        static GoodbyeMessage reference_goodbye_message;
        if (reference_relay_state.relays.size() == 0)
        {
            SendAKeyDistributionMessageWhichGivesAQuarterKeyToTheChosenRelay();
            GenerateGoodbyeMessage();
            SAVE_CACHED_DATA;
            reference_goodbye_message = goodbye_message;
        }
        else
        {
            LOAD_CACHED_DATA;
            goodbye_message = reference_goodbye_message;
        }

        goodbye_message_hash = goodbye_message.GetHash160();
    }

    void GenerateGoodbyeMessage()
    {
        goodbye_message = AGoodbyeMessageWithABadSecret();
        data->StoreMessage(goodbye_message);
    }

    GoodbyeMessage AGoodbyeMessageWithABadSecret()
    {
        goodbye_message = relay->GenerateGoodbyeMessage(*data);
        goodbye_message.encrypted_key_sixteenths[0][2] += 1;
        goodbye_message.Sign(*data);
        return goodbye_message;
    }

    GoodbyeComplaint ComplaintAboutBadGoodbyeMessage()
    {
        GoodbyeComplaint complaint;
        complaint.key_sharer_position = 0;
        complaint.position_of_bad_encrypted_key_sixteenth = 2;
        complaint.Populate(goodbye_message, *data);
        complaint.Sign(*data);
        data->StoreMessage(complaint);
        return complaint;
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret,
       RejectsAGoodbyeComplaintThatArrivesAfterASuccessionCompletedMessage)
{
    relay_message_handler->succession_handler.send_goodbye_complaints = false;
    relay_message_handler->Handle(goodbye_message, &peer);
    SuccessionCompletedMessage succession_completed_message;
    succession_completed_message.goodbye_message_hash = goodbye_message.GetHash160();
    data->StoreMessage(succession_completed_message);

    auto successor = goodbye_message.GetSuccessor(*data);
    successor->hashes.succession_completed_message_hashes.push_back(succession_completed_message.GetHash160());
    auto complaint = ComplaintAboutBadGoodbyeMessage();
    relay_message_handler->Handle(complaint, &peer);

    ASSERT_FALSE(VectorContainsEntry(builder->accepted_messages, complaint.GetHash160()));
}

TEST_F(ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret,
       FailsValidationOfADurationWithoutResponseIfAGoodbyeComplaintHasBeenProcessed)
{
    relay_message_handler->succession_handler.send_goodbye_complaints = false;
    relay_message_handler->Handle(goodbye_message, &peer);

    auto complaint = ComplaintAboutBadGoodbyeMessage();
    relay_message_handler->Handle(complaint, &peer);

    DurationWithoutResponse duration;
    duration.message_hash = goodbye_message.GetHash160();
    ASSERT_FALSE(relay_message_handler->ValidateDurationWithoutResponse(duration));
}


class ARelayMessageHandlerWhichHasProcessedAGoodbyeMessageWithABadSecret :
        public ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret::SetUp();
        relay_message_handler->succession_handler.send_secret_recovery_messages = false;
        relay_message_handler->Handle(goodbye_message, &peer);
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAGoodbyeMessageWithABadSecret,
       SendsAGoodbyeComplaintIfTheSuccessorKeyIsAvailable)
{
    vector<uint160> complaint_hashes = relay->hashes.goodbye_complaint_hashes;
    ASSERT_THAT(complaint_hashes.size(), Eq(1));

    GoodbyeComplaint complaint = data->GetMessage(complaint_hashes[0]);
    ASSERT_THAT(complaint.key_sharer_position, Eq(0));
    ASSERT_THAT(complaint.position_of_bad_encrypted_key_sixteenth, Eq(2));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAGoodbyeMessageWithABadSecret,
       EncodesAValidGoodbyeComplaint)
{
    vector<uint160> complaint_hashes = relay->hashes.goodbye_complaint_hashes;
    ASSERT_THAT(complaint_hashes.size(), Eq(1));
    ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, complaint_hashes[0]));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAGoodbyeMessageWithABadSecret,
       DoesntEncodeADurationWithoutResponseIfItHasProcessedAValidGoodbyeComplaint)
{
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();
    DurationWithoutResponse duration;
    duration.message_hash = goodbye_message.GetHash160();
    ASSERT_FALSE(VectorContainsEntry(builder->accepted_messages, duration.GetHash160()));
}


class ARelayMessageHandlerInBlockValidationModeWhichHasProcessedAGoodbyeMessageWithABadSecret :
        public ARelayMessageHandlerWhichHasProcessedAGoodbyeMessageWithABadSecret
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessage::SetUp();
        relay_message_handler->mode = BLOCK_VALIDATION;
        GenerateGoodbyeMessage();
        relay_message_handler->Handle(goodbye_message, &peer);
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedAGoodbyeMessageWithABadSecret::TearDown();
    }
};

TEST_F(ARelayMessageHandlerInBlockValidationModeWhichHasProcessedAGoodbyeMessageWithABadSecret,
       DoesntSendAGoodbyeComplaint)
{
    vector<uint160> complaint_hashes = relay->hashes.goodbye_complaint_hashes;
    ASSERT_THAT(complaint_hashes.size(), Eq(0));
}


class ARelayMessageHandlerWhichHasProcessedAValidGoodbyeComplaint :
        public ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret::SetUp();
        relay_state->remove_dead_relays = false;
        relay_message_handler->Handle(goodbye_message, &peer);
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeComplaint,
       GeneratesAnObituarySpecifyingTheComplaintAsTheReasonForTheDeadRelayLeaving)
{
    ASSERT_THAT(relay->hashes.obituary_hash, Ne(0));

    Obituary obituary = data->GetMessage(relay->hashes.obituary_hash);
    ASSERT_THAT(obituary.reason_for_leaving, Eq(MISBEHAVED));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidGoodbyeComplaint,
       SendsSecretRecoveryMessagesInResponseToTheObituary)
{
    vector<uint160> recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
    ASSERT_THAT(recovery_message_hashes.size(), Eq(4));
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
        relay_message_handler->admission_handler.send_key_distribution_complaints = false;
        relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);
        relay_message_handler->admission_handler.send_key_distribution_complaints = true;
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessageWithABadSecret,
       RejectsAKeyDistributionComplaintWhichRefersToANonExistentSecret)
{
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(), KEY_QUARTER_HOLDERS, 1, *data);
    complaint.position_of_secret = 51;
    complaint.Sign(*data);

    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
    ASSERT_TRUE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessageWithABadSecret,
       AcceptsAKeyDistributionComplaintAboutTheBadSecret)
{
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(), KEY_QUARTER_HOLDERS, 1, *data);
    complaint.Sign(*data);

    bool complaint_is_valid = complaint.IsValid(*data);
    ASSERT_THAT(complaint_is_valid, Eq(true));

    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
    ASSERT_FALSE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionMessageWithABadSecret,
       EncodesAValidKeyDistributionComplaint)
{
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(), KEY_QUARTER_HOLDERS, 1, *data);
    complaint.Sign(*data);
    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
    ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, complaint.GetHash160()));
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
        DECLARE_CACHED_DATA;

        if (reference_relay_state.relays.size() == 0)
        {
            DoSetUp();

            SAVE_CACHED_DATA;
            reference_complaint = complaint;
        }
        else
        {
            LOAD_CACHED_DATA;
            complaint = reference_complaint;
        }
    }

    void DoSetUp()
    {
        EnsureThereIsSomethingToComplainAboutAndThatTheChosenRelayIsHoldingSecrets();
        relay_message_handler->admission_handler.send_key_distribution_complaints = false;
        relay_message_handler->HandleMessage(GetDataStream(key_distribution_message), &peer);
        complaint.Populate(key_distribution_message.GetHash160(), 0, 1, *data);
        complaint.Sign(*data);
        data->StoreMessage(complaint);
        relay_message_handler->admission_handler.send_key_distribution_complaints = true;
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

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet,
       AcceptsAKeyDistributionComplaintAboutTheBadPoint)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    bool complaint_is_valid = complaint.IsValid(*data);
    ASSERT_THAT(complaint_is_valid, Eq(true));

    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
    ASSERT_FALSE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet,
       RejectsAKeyDistributionComplaintWhichArrivesAfterADurationWithoutResponseAfterTheKeyDistributionMessage)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay->hashes.key_distribution_complaint_hashes = vector<uint160>();
    relay_message_handler->admission_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
    ASSERT_TRUE(IsRejected(complaint));
}

TEST_F(ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet,
       FailsValidationOfADurationWithoutResponseAfterTheKeyDistributionComplaintHasBeenProcessed)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    relay->hashes.key_distribution_complaint_hashes = vector<uint160>();

    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
    ASSERT_FALSE(IsRejected(complaint));

    DurationWithoutResponse duration;
    duration.message_hash = key_distribution_message.GetHash160();
    ASSERT_FALSE(relay_message_handler->ValidateDurationWithoutResponse(duration));
}

TEST_F(ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet,
       AddsAnObituaryForTheOffendingRelayWhenProcessingAValidKeyDistributionComplaint)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
    ASSERT_THAT(relay->hashes.obituary_hash, Ne(0));
}


class ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButNoSecretRecoveryMessages :
        public ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::SetUp();
        DoCachedSetUp();
    }

    void DoCachedSetUp()
    {
        DECLARE_CACHED_DATA;

        if (reference_relay_state.relays.size() == 0)
        {
            relay_message_handler->succession_handler.send_secret_recovery_messages = false;
            relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
            relay_message_handler->succession_handler.send_secret_recovery_messages = true;
            SAVE_CACHED_DATA;
        }
        else
        {
            LOAD_CACHED_DATA;
        }
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButNoSecretRecoveryMessages,
       AssignsTheObituaryAsATaskToTheQuarterHolders)
{
    for (auto quarter_holder : relay->QuarterHolders(*data))
        ASSERT_TRUE(VectorContainsEntry(quarter_holder->tasks, relay->hashes.obituary_hash));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButNoSecretRecoveryMessages,
       RemovesTheObituaryFromTheQuarterHoldersTasksWhenProcessingASecretRecoveryMessage)
{
    for (auto quarter_holder : relay->QuarterHolders(*data))
    {
        ASSERT_TRUE(VectorContainsEntry(quarter_holder->tasks, relay->hashes.obituary_hash));
        relay_message_handler->succession_handler.SendSecretRecoveryMessage(relay, quarter_holder);
        ASSERT_FALSE(VectorContainsEntry(quarter_holder->tasks, relay->hashes.obituary_hash));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButNoSecretRecoveryMessages,
       ReaddsTheObituaryToTheQuarterHoldersTasksWhenProcessingASecretRecoveryComplaint)
{
    relay_state->remove_dead_relays = false;
    relay_message_handler->succession_handler.send_succession_completed_messages = false;
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;

    for (auto quarter_holder : relay->QuarterHolders(*data))
    {
        relay_message_handler->succession_handler.SendSecretRecoveryMessage(relay, quarter_holder);
        ASSERT_FALSE(VectorContainsEntry(quarter_holder->tasks, relay->hashes.obituary_hash));
        SecretRecoveryComplaint complaint;
        complaint.secret_recovery_message_hash = relay->hashes.secret_recovery_message_hashes.back();
        relay_message_handler->succession_handler.AcceptSecretRecoveryComplaint(complaint);
        ASSERT_TRUE(VectorContainsEntry(quarter_holder->tasks, relay->hashes.obituary_hash));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButNoSecretRecoveryMessages,
       RemovesTheSecretRecoveryMessageHashFromTheDeadRelayWhenProcessingASecretRecoveryComplaint)
{
    relay_state->remove_dead_relays = false;
    relay_message_handler->succession_handler.send_succession_completed_messages = false;
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;

    for (auto quarter_holder : relay->QuarterHolders(*data))
    {
        relay_message_handler->succession_handler.SendSecretRecoveryMessage(relay, quarter_holder);
        SecretRecoveryComplaint complaint;
        complaint.secret_recovery_message_hash = relay->hashes.secret_recovery_message_hashes.back();
        relay_message_handler->succession_handler.AcceptSecretRecoveryComplaint(complaint);
        ASSERT_FALSE(VectorContainsEntry(relay->hashes.secret_recovery_message_hashes,
                                         complaint.secret_recovery_message_hash));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButNoSecretRecoveryMessages,
       AssignsTheFourthSecretRecoveryMessageAsATaskToTheSuccessor)
{
    relay_message_handler->succession_handler.send_succession_completed_messages = false;
    relay_message_handler->succession_handler.SendSecretRecoveryMessages(relay);
    auto last_recovery_message_hash = relay->hashes.secret_recovery_message_hashes.back();
    auto successor = relay_state->GetRelayByNumber(relay->SuccessorNumber(*data));
    ASSERT_TRUE(VectorContainsEntry(successor->tasks, last_recovery_message_hash));
}



class ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButHasntYetRemovedTheDeadRelay :
   public ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::SetUp();
        DoCachedSetUp();
    }

    void DoCachedSetUp()
    {
        DECLARE_CACHED_DATA;

        if (reference_relay_state.relays.size() == 0)
        {
            relay_state->remove_dead_relays = false;
            relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
            relay_state->remove_dead_relays = true;
            SAVE_CACHED_DATA;
        }
        else
        {
            LOAD_CACHED_DATA;
        }
    }
    
    virtual void TearDown()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::TearDown();
    }
};

class ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay :
        public ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButHasntYetRemovedTheDeadRelay
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButHasntYetRemovedTheDeadRelay::SetUp();
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButHasntYetRemovedTheDeadRelay::TearDown();
    }

    void UnprocessSecretRecoveryMessages()
    {
        UnscheduleTasksToCheckForResponsesToSecretRecoveryMessages();
        relay->hashes.secret_recovery_message_hashes = vector<uint160>();
        auto successor = relay_state->GetRelayByNumber(relay->SuccessorNumber(*data));
        successor->hashes.succession_completed_message_hashes.resize(0);
    }

    void UnscheduleTasksToCheckForResponsesToSecretRecoveryMessages()
    {
        auto &scheduler = relay_message_handler->succession_handler.scheduler;
        auto scheduled_tasks = scheduler.TasksScheduledForTime("secret_recovery", GetTimeMicros() + RESPONSE_WAIT_TIME);
        for (auto recovery_message_hash : relay->hashes.secret_recovery_message_hashes)
            EraseEntryFromVector(recovery_message_hash, scheduled_tasks);
        scheduler.scheduledata[scheduled_tasks].Location("secret_recovery") = GetTimeMicros() + RESPONSE_WAIT_TIME;
    }

};

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       SendsSecretRecoveryMessagesFromEachQuarterHolderWhosePrivateSigningKeyIsKnown)
{
    vector<uint160> secret_recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
    ASSERT_THAT(secret_recovery_message_hashes.size(), Eq(4));

    SecretRecoveryMessage secret_recovery_message;
    for (auto secret_recovery_message_hash : secret_recovery_message_hashes)
    {
        secret_recovery_message = data->GetMessage(secret_recovery_message_hash);
        ASSERT_TRUE(VectorContainsEntry(relay->holders.key_quarter_holders, secret_recovery_message.quarter_holder_number));
        ASSERT_TRUE(peer.HasBeenInformedAbout("relay", "secret_recovery", secret_recovery_message));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       SendsASuccessionCompletedMessageIfTheSecretsHeldAreRecoveredFromTheSecretRecoveryMessages)
{
    SecretRecoveryMessage secret_recovery_message = data->GetMessage(relay->hashes.secret_recovery_message_hashes[0]);
    auto successor = secret_recovery_message.GetSuccessor(*data);
    ASSERT_THAT(successor->hashes.succession_completed_message_hashes.size(), Eq(1));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       SchedulesATaskToCheckWhichQuarterHoldersHaveRespondedToTheObituary)
{
    ASSERT_TRUE(relay_message_handler->succession_handler.scheduler.TaskIsScheduledForTime(
                "obituary", relay->hashes.obituary_hash, TEST_START_TIME + RESPONSE_WAIT_TIME));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       GeneratesObituariesForKeyQuarterHoldersWhoHaveNotRespondedToTheObituaryWhenCompletingTheScheduledTask)
{
    UnprocessSecretRecoveryMessages();
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;

    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (auto quarter_holder : relay->QuarterHolders(*data))
        ASSERT_THAT(quarter_holder->hashes.obituary_hash, Ne(0));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       EncodesDurationsWithoutResponsesFromKeyQuarterHoldersWhoHaveNotRespondedToTheObituary)
{
    UnprocessSecretRecoveryMessages();
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;

    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (auto quarter_holder : relay->QuarterHolders(*data))
    {
        auto duration = GetDurationWithoutResponseFromRelay(relay->hashes.obituary_hash, quarter_holder->number);
        ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, duration.GetHash160()));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       DoesntGenerateObituariesForKeyQuarterHoldersWhoHaveRespondedToTheObituaryWhenCompletingTheScheduledTask)
{
    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (auto quarter_holder_number : relay->holders.key_quarter_holders)
    {
        auto quarter_holder = relay_message_handler->relay_state.GetRelayByNumber(quarter_holder_number);
        ASSERT_THAT(quarter_holder->hashes.obituary_hash, Eq(0));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       HandlesTheSecretRecoveryMessagesWhichItGeneratesInResponseToTheObituary)
{
    vector<uint160> secret_recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
    for (auto secret_recovery_message_hash : secret_recovery_message_hashes)
    {
        bool handled = msgdata[secret_recovery_message_hash]["handled"];
        ASSERT_THAT(handled, Eq(true));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       SchedulesTasksToCheckForAResponseFromTheSuccessorWhenHandlingTheFourthSecretRecoveryMessage)
{
    auto &scheduler = relay_message_handler->succession_handler.scheduler;
    for (uint64_t i = 0; i < 4; i++)
    {
        auto secret_recovery_message_hash = relay->hashes.secret_recovery_message_hashes[i];
        if (i < 3)
            ASSERT_FALSE(scheduler.TaskIsScheduledForTime("secret_recovery", secret_recovery_message_hash,
                                                          TEST_START_TIME + RESPONSE_WAIT_TIME));
        else
            ASSERT_TRUE(scheduler.TaskIsScheduledForTime("secret_recovery", secret_recovery_message_hash,
                                                         TEST_START_TIME + RESPONSE_WAIT_TIME));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       EncodesAValidSecretRecoveryMessage)
{
    vector<uint160> secret_recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
    for (auto secret_recovery_message_hash : secret_recovery_message_hashes)
        ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, secret_recovery_message_hash));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       EncodesTheDurationWithoutResponseIfThereAreNoResponsesToTheFourthSecretRecoveryMessage)
{
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (uint64_t i = 0; i < 4; i++)
    {
        auto secret_recovery_message_hash = relay->hashes.secret_recovery_message_hashes[i];
        DurationWithoutResponse duration = GetDurationWithoutResponseAfter(secret_recovery_message_hash);
        if (i == 3)
            ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, duration.GetHash160()));
        else
            ASSERT_FALSE(VectorContainsEntry(builder->accepted_messages, duration.GetHash160()));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       MarksTheSuccessorAsNotRespondingIfThereAreNoResponsesToTheFourthSecretRecoveryMessage)
{
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();
    ASSERT_TRUE(RelayIsDeadWithReason(relay->SuccessorNumber(*data), NOT_RESPONDING));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAnObituaryButHasntYetRemovedTheDeadRelay,
       RemovesTheObituaryFromTheQuarterHoldersTasksWhenProcessingASecretRecoveryMessage)
{
    for (auto quarter_holder : relay->QuarterHolders(*data))
        ASSERT_FALSE(VectorContainsEntry(quarter_holder->tasks, relay->hashes.obituary_hash));
}


class ARelayMessageHandlerInBlockValidationModeWhichHasProcessedAValidKeyDistributionComplaint :
        public ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::SetUp();
        relay_message_handler->mode = BLOCK_VALIDATION;
        relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::TearDown();
    }
};

TEST_F(ARelayMessageHandlerInBlockValidationModeWhichHasProcessedAValidKeyDistributionComplaint,
       DoesntScheduleATaskToCheckWhichQuarterHoldersHaveRespondedToTheObituary)
{
    ASSERT_FALSE(relay_message_handler->succession_handler.scheduler.TaskIsScheduledForTime(
                 "obituary", relay->hashes.obituary_hash, TEST_START_TIME + RESPONSE_WAIT_TIME));
}

TEST_F(ARelayMessageHandlerInBlockValidationModeWhichHasProcessedAValidKeyDistributionComplaint,
       DoesntSendSecretRecoveryMessages)
{
    vector<uint160> secret_recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
    ASSERT_THAT(secret_recovery_message_hashes.size(), Eq(0));
}

TEST_F(ARelayMessageHandlerInBlockValidationModeWhichHasProcessedAValidKeyDistributionComplaint,
       DoesntScheduleTasksToCheckForResponsesWhenHandlingTheFourthSecretRecoveryMessage)
{
    auto &succession_handler = relay_message_handler->succession_handler;
    for (auto quarter_holder : relay->QuarterHolders(*data))
    {
        auto recovery_message = succession_handler.GenerateSecretRecoveryMessage(relay, quarter_holder);
        relay_message_handler->Handle(recovery_message, NULL);
    }
    auto secret_recovery_message_hash = relay->hashes.secret_recovery_message_hashes.back();
    SecretRecoveryMessage recovery_message = data->GetMessage(secret_recovery_message_hash);

    ASSERT_FALSE(IsRejected(recovery_message));
    ASSERT_FALSE(succession_handler.scheduler.TaskIsScheduledForTime("secret_recovery", secret_recovery_message_hash,
                                                                     TEST_START_TIME + RESPONSE_WAIT_TIME));
}


class ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage :
    public ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet
{
public:
    SecretRecoveryFailureMessage failure_message;
    SecretRecoveryComplaint recovery_complaint;
    SecretRecoveryMessage recovery_message;

    virtual void SetUp()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::SetUp();
        DoCachedSetUp();

        failure_message.Populate(relay->hashes.secret_recovery_message_hashes, *data);

        vector<uint160> recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
        recovery_message = data->GetMessage(recovery_message_hashes[0]);
        recovery_complaint.Populate(recovery_message, 0, 0, *data);
    }

    void DoCachedSetUp()
    {
        DECLARE_CACHED_DATA;

        if (reference_relay_state.relays.size() == 0)
        {
            relay_message_handler->succession_handler.send_succession_completed_messages = false;
            relay_message_handler->HandleMessage(GetDataStream(complaint), &peer);
            relay_message_handler->succession_handler.send_succession_completed_messages = true;
            SAVE_CACHED_DATA;
        }
        else
        {
            LOAD_CACHED_DATA;
        }
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAValidComplaintAboutABadPointFromTheRelaysPublicKeySet::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage,
       RejectsASecretRecoveryFailureMessageWithTheWrongNumberOfRecoveryMessageHashes)
{
    failure_message.recovery_message_hashes.push_back(0);
    failure_message.Sign(*data);
    relay_message_handler->Handle(failure_message, NULL);
    ASSERT_TRUE(IsRejected(failure_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage,
       RejectsASecretRecoveryFailureMessageWithAnImpossibleKeySharerPosition)
{
    failure_message.key_sharer_position = 17;
    failure_message.Sign(*data);
    relay_message_handler->Handle(failure_message, NULL);
    ASSERT_TRUE(IsRejected(failure_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage,
       RejectsASecretRecoveryFailureMessageWithAnImpossibleSharedSecretQuarterPosition)
{
    failure_message.shared_secret_quarter_position = 17;
    failure_message.Sign(*data);
    relay_message_handler->Handle(failure_message, NULL);
    ASSERT_TRUE(IsRejected(failure_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage,
       RejectsASecretRecoveryComplaintWithAnImpossibleKeySharerPosition)
{
    recovery_complaint.position_of_key_sharer = 17;
    recovery_complaint.Sign(*data);
    relay_message_handler->Handle(recovery_complaint, NULL);
    ASSERT_TRUE(IsRejected(recovery_complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage,
       RejectsASecretRecoveryComplaintWithAnImpossibleBadEncryptedSecretPosition)
{
    recovery_complaint.position_of_bad_encrypted_secret = 17;
    recovery_complaint.Sign(*data);
    relay_message_handler->Handle(recovery_complaint, NULL);
    ASSERT_TRUE(IsRejected(recovery_complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage,
       RejectsASecretRecoveryComplaintAboutAGoodSecret)
{
    recovery_complaint.Sign(*data);
    relay_message_handler->Handle(recovery_complaint, NULL);
    ASSERT_TRUE(IsRejected(recovery_complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage,
       RejectsASecretRecoveryComplaintWithTheWrongPrivateReceivingKey)
{
    recovery_complaint.private_receiving_key += 1;
    recovery_complaint.Sign(*data);
    relay_message_handler->Handle(recovery_complaint, NULL);
    ASSERT_TRUE(IsRejected(recovery_complaint));
}



class ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages :
        public ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButNoSecretRecoveryMessages
{
public:
    SecretRecoveryMessage last_recovery_message;
    uint160 last_recovery_message_hash;
    Relay *successor;

    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButNoSecretRecoveryMessages::SetUp();
        SetUpRecoveryMessagesAndForgetKeyQuartersHeldByDeadRelay();
    }

    void SetUpRecoveryMessagesAndForgetKeyQuartersHeldByDeadRelay()
    {
        for (auto quarter_holder : relay->QuarterHolders(*data))
        {
            if (quarter_holder == relay->QuarterHolders(*data).back())
                break;
            relay_message_handler->succession_handler.SendSecretRecoveryMessage(relay, quarter_holder);
        }
        SetUpLastRecoveryMessage();
        successor = last_recovery_message.GetSuccessor(*data);
        ForgetKeyQuartersHeldByDeadRelay();
    }

    void SetUpLastRecoveryMessage()
    {
        auto last_quarter_holder = relay->QuarterHolders(*data).back();
        last_recovery_message = relay_message_handler->succession_handler.GenerateSecretRecoveryMessage(relay, last_quarter_holder);
        last_recovery_message_hash = last_recovery_message.GetHash160();
        data->StoreMessage(last_recovery_message);
    }

    void ForgetKeyQuartersHeldByDeadRelay()
    {
        for (auto &existing_relay : relay_message_handler->relay_state.relays)
            if (VectorContainsEntry(existing_relay.holders.key_quarter_holders, relay->number))
                ForgetKeyQuartersOfGivenRelayHeldByDeadRelay(existing_relay);
    }

    void ForgetKeyQuartersOfGivenRelayHeldByDeadRelay(Relay &given_relay)
    {
        auto key_quarter_position = PositionOfEntryInVector(relay->number, given_relay.holders.key_quarter_holders);

        for (auto key_sixteenth_position = key_quarter_position * 4;
             key_sixteenth_position < 4 + key_quarter_position * 4;
             key_sixteenth_position++)
        {
            keydata.Delete(given_relay.PublicKeySixteenths()[key_sixteenth_position]);
        }
    }

    bool KeyQuartersHeldByDeadRelayAreKnown()
    {
        for (auto &existing_relay : relay_message_handler->relay_state.relays)
            if (VectorContainsEntry(existing_relay.holders.key_quarter_holders, relay->number))
                if (not KeyQuartersOfGivenRelayHeldByDeadRelayAreKnown(existing_relay))
                    return false;
        return true;
    }

    bool KeyQuartersOfGivenRelayHeldByDeadRelayAreKnown(Relay &given_relay)
    {
        auto key_quarter_position = PositionOfEntryInVector(relay->number, given_relay.holders.key_quarter_holders);
        for (auto key_sixteenth_position = key_quarter_position * 4; key_sixteenth_position < key_quarter_position * 5;
                                                                     key_sixteenth_position++)
            if (not keydata[given_relay.PublicKeySixteenths()[key_sixteenth_position]].HasProperty("privkey"))
                return false;
        return true;
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedAKeyDistributionComplaintButNoSecretRecoveryMessages::TearDown();
    }

    SecretRecoveryMessage BadSecretRecoveryMessage();

    SecretRecoveryComplaint ComplaintAboutBadSecretRecoveryMessage();
};

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       RejectsASecretRecoveryMessageWithTheWrongNumberOfComponents)
{
    SecretRecoveryMessage secret_recovery_message = last_recovery_message;
    secret_recovery_message.key_quarter_positions.push_back(0);
    secret_recovery_message.Sign(*data);

    relay_message_handler->Handle(secret_recovery_message, NULL);
    ASSERT_TRUE(IsRejected(secret_recovery_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       RejectsASecretRecoveryMessageWhichContainsIncorrectRelayNumbers)
{
    SecretRecoveryMessage secret_recovery_message = last_recovery_message;
    while (VectorContainsEntry(relay->holders.key_quarter_holders, secret_recovery_message.quarter_holder_number))
        secret_recovery_message.quarter_holder_number += 1;
    secret_recovery_message.Sign(*data);

    relay_message_handler->Handle(secret_recovery_message, NULL);
    ASSERT_TRUE(IsRejected(secret_recovery_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       RejectsASecretRecoveryMessageWhichReferencesNonExistentSecrets)
{
    SecretRecoveryMessage secret_recovery_message = last_recovery_message;
    secret_recovery_message.key_quarter_positions[0] = 17;
    secret_recovery_message.Sign(*data);

    relay_message_handler->Handle(secret_recovery_message, NULL);
    ASSERT_TRUE(IsRejected(secret_recovery_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       RejectsASecretRecoveryMessageWhoseReferencedKeyQuarterSharersDidntShareKeyQuartersWithTheDeadRelay)
{
    SecretRecoveryMessage secret_recovery_message = last_recovery_message;
    secret_recovery_message.key_quarter_sharers[0] = 17;
    secret_recovery_message.Sign(*data);

    relay_message_handler->Handle(secret_recovery_message, NULL);
    ASSERT_TRUE(IsRejected(secret_recovery_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       RejectsASecretRecoveryMessageWhichDoesntHaveTheNecessaryEncryptedKeySixteenths)
{
    SecretRecoveryMessage secret_recovery_message = last_recovery_message;
    secret_recovery_message.key_quarter_sharers.resize(0);
    secret_recovery_message.key_quarter_positions.resize(0);
    secret_recovery_message.quartets_of_encrypted_shared_secret_quarters.resize(0);
    secret_recovery_message.quartets_of_points_corresponding_to_shared_secret_quarters.resize(0);
    secret_recovery_message.Sign(*data);

    relay_message_handler->Handle(secret_recovery_message, NULL);
    ASSERT_TRUE(IsRejected(secret_recovery_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       RejectsASecretRecoveryMessageWhichArrivesAfterADurationWithoutResponseAfterTheObituary)
{
    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    relay_message_handler->Handle(last_recovery_message, NULL);
    ASSERT_TRUE(IsRejected(last_recovery_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       DoesntRejectASecretRecoveryMessageWhichDoesntArriveAfterADurationWithoutResponseAfterTheKeyDistributionComplaint)
{
    relay_message_handler->Handle(last_recovery_message, NULL);
    ASSERT_FALSE(IsRejected(last_recovery_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       ReconstructsKeyPartsHeldByTheDeadRelayIfTheSuccessorsPrivateSigningKeyIsAvailableWhenProcessingTheFourth)
{
    ASSERT_FALSE(KeyQuartersHeldByDeadRelayAreKnown());
    relay_message_handler->Handle(last_recovery_message, NULL);
    ASSERT_TRUE(KeyQuartersHeldByDeadRelayAreKnown());
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
        SendsASecretRecoveryComplaintIfASecretRecoveryMessageContainsASecretWhichFailsDecryption)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;

    last_recovery_message.quartets_of_encrypted_shared_secret_quarters[0][1] += 1;
    last_recovery_message.Sign(*data);
    data->StoreMessage(last_recovery_message);
    relay_message_handler->Handle(last_recovery_message, NULL);

    vector<uint160> complaints = relay->hashes.secret_recovery_complaint_hashes;
    ASSERT_THAT(complaints.size(), Eq(1));

    SecretRecoveryComplaint complaint = data->GetMessage(complaints[0]);
    ASSERT_THAT(complaint.position_of_key_sharer, Eq(0));
    ASSERT_THAT(complaint.position_of_bad_encrypted_secret, Eq(1));
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", complaint));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       DoesntSendASecretRecoveryComplaintInBlockValidationMode)
{
    SecretRecoveryMessage secret_recovery_message = last_recovery_message;
    secret_recovery_message.quartets_of_encrypted_shared_secret_quarters[0][1] += 1;
    secret_recovery_message.Sign(*data);

    relay_message_handler->mode = BLOCK_VALIDATION;
    relay_message_handler->Handle(secret_recovery_message, NULL);

    ASSERT_THAT(relay->hashes.secret_recovery_complaint_hashes.size(), Eq(0));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       GeneratesAnObituaryForTheBadKeyQuarterHolderInResponseToAValidSecretRecoveryComplaint)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    last_recovery_message.quartets_of_encrypted_shared_secret_quarters[0][1] += 1;
    last_recovery_message.Sign(*data);

    relay_message_handler->Handle(last_recovery_message, NULL);
    auto key_quarter_holder = last_recovery_message.GetKeyQuarterHolder(*data);
    ASSERT_THAT(key_quarter_holder->hashes.obituary_hash, Ne(0));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       EncodesAValidSecretRecoveryComplaint)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    last_recovery_message.quartets_of_encrypted_shared_secret_quarters[0][1] += 1;
    last_recovery_message.Sign(*data);
    relay_message_handler->Handle(last_recovery_message, NULL);

    vector<uint160> complaints = relay->hashes.secret_recovery_complaint_hashes;
    ASSERT_THAT(complaints.size(), Eq(1));
    ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, complaints[0]));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       FailsValidationOfADurationWithoutResponseAfterASecretRecoveryMessageIfThereHasBeenASecretRecoveryComplaint)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    last_recovery_message.quartets_of_encrypted_shared_secret_quarters[0][1] += 1;
    last_recovery_message.Sign(*data);
    data->StoreMessage(last_recovery_message);
    relay_message_handler->Handle(last_recovery_message, NULL);

    DurationWithoutResponse duration;
    duration.message_hash = last_recovery_message.GetHash160();
    ASSERT_FALSE(relay_message_handler->ValidateDurationWithoutResponse(duration));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       DoesntFailValidationOfADurationWithoutResponseAfterASecretRecoveryMessageIfThereHasntBeenASecretRecoveryComplaint)
{
    relay_message_handler->mode = BLOCK_VALIDATION;
    relay_message_handler->Handle(last_recovery_message, NULL);

    DurationWithoutResponse duration;
    duration.message_hash = last_recovery_message_hash;
    ASSERT_TRUE(relay_message_handler->ValidateDurationWithoutResponse(duration));
}

SecretRecoveryMessage
ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages::BadSecretRecoveryMessage()
{
    SecretRecoveryMessage last_secret_recovery_message = last_recovery_message;
    last_secret_recovery_message.quartets_of_encrypted_shared_secret_quarters[0][1] += 1;
    last_secret_recovery_message.Sign(*data);
    data->StoreMessage(last_secret_recovery_message);
    return last_secret_recovery_message;
}


SecretRecoveryComplaint
ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages::ComplaintAboutBadSecretRecoveryMessage()
{
    SecretRecoveryComplaint complaint;
    complaint.Populate(BadSecretRecoveryMessage(), 0, 1, *data);
    complaint.Sign(*data);
    data->StoreMessage(complaint);
    return complaint;
}

TEST_F(ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages,
       RejectsASecretRecoveryComplaintWhichArrivesAfterASuccessionCompletedMessage)
{
    relay_message_handler->succession_handler.send_secret_recovery_complaints = false;
    auto bad_recovery_message = BadSecretRecoveryMessage();
    relay_message_handler->Handle(bad_recovery_message, NULL);

    auto succession_completed_message = successor->GenerateSuccessionCompletedMessage(bad_recovery_message, *data);
    data->StoreMessage(succession_completed_message);
    relay_message_handler->Handle(succession_completed_message, NULL);

    auto complaint = ComplaintAboutBadSecretRecoveryMessage();
    relay_message_handler->Handle(complaint, NULL);
    ASSERT_TRUE(IsRejected(complaint));
}


class ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter :
        public ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages
{
public:
    SecretRecoveryMessage bad_recovery_message;
    bool suppress_audit_messages{false};

    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages::SetUp();
        GenerateAndHandleRecoveryMessageWithBadSharedSecretQuarter();
    }

    void GenerateAndHandleRecoveryMessageWithBadSharedSecretQuarter()
    {
        bad_recovery_message = GenerateRecoveryMessageWithBadSharedSecretQuarter();
        data->StoreMessage(bad_recovery_message);

        if (suppress_audit_messages)
            relay_message_handler->succession_handler.send_audit_messages = false;

        relay_message_handler->Handle(bad_recovery_message, &peer);
    }

    SecretRecoveryMessage GenerateRecoveryMessageWithBadSharedSecretQuarter()
    {
        SecretRecoveryMessage recovery_message = data->GetMessage(last_recovery_message_hash);
        auto successor = recovery_message.GetSuccessor(*data);
        Point bad_shared_secret_quarter(CBigNum(12345));
        PopulateRecoveryMessageWithABadSharedSecretQuarter(recovery_message, successor, bad_shared_secret_quarter);
        recovery_message.Sign(*data);
        return recovery_message;
    }

    void PopulateRecoveryMessageWithABadSharedSecretQuarter(SecretRecoveryMessage &recovery_message,
                                                            Relay *successor, Point bad_shared_secret_quarter)
    {
        CBigNum encoded_bad_shared_secret_quarter = StorePointInBigNum(bad_shared_secret_quarter);
        uint256 encrypted_bad_shared_secret_quarter = successor->EncryptSecret(encoded_bad_shared_secret_quarter);
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
    ASSERT_THAT(relay->hashes.secret_recovery_failure_message_hashes.size(), Ne(0));

    uint160 failure_message_hash = relay->hashes.secret_recovery_failure_message_hashes[0];
    SecretRecoveryFailureMessage failure_message = data->GetMessage(failure_message_hash);

    ASSERT_THAT(failure_message.key_sharer_position, Eq(0));
    ASSERT_THAT(failure_message.shared_secret_quarter_position, Eq(2));
    ASSERT_TRUE(peer.HasBeenInformedAbout("relay", failure_message));
}


class ARelayMessageHandlerInBlockValidationModeWithFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter :
        public ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedAValidKeyDistributionComplaintAndThreeSecretRecoveryMessages::SetUp();
        relay_message_handler->mode = BLOCK_VALIDATION;
        GenerateAndHandleRecoveryMessageWithBadSharedSecretQuarter();
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter::TearDown();
    }
};

TEST_F(ARelayMessageHandlerInBlockValidationModeWithFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter,
       DoesntSendASecretRecoveryFailureMessage)
{
    ASSERT_THAT(relay->hashes.secret_recovery_failure_message_hashes.size(), Eq(0));
}


class ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage :
        public ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter
{
public:
    uint160 failure_message_hash;
    SecretRecoveryFailureMessage failure_message;

    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter::SetUp();
        failure_message_hash = relay->hashes.secret_recovery_failure_message_hashes[0];
        failure_message = data->GetMessage(failure_message_hash);
    }

    Relay *GetKeySharer()
    {
        uint64_t key_sharer_number = NumberOfARelayWhoSharedAQuarterKeyWithChosenRelay();
        return relay_message_handler->relay_state.GetRelayByNumber(key_sharer_number);
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage,
       EncodesTheSecretRecoveryFailureMessageInTheChain)
{
    ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, failure_message_hash));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage,
       SendsRecoveryFailureAuditMessagesFromEachQuarterHolderWhoseKeyIsAvailable)
{
    vector<uint160> audit_message_hashes = relay->hashes.recovery_failure_audit_message_hashes;

    ASSERT_THAT(audit_message_hashes.size(), Eq(4));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage,
       EncodesRecoveryFailureAuditMessages)
{
    vector<uint160> audit_message_hashes = relay->hashes.recovery_failure_audit_message_hashes;
    for (auto audit_message_hash : audit_message_hashes)
        ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, audit_message_hash));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage,
       DoesntSendRecoveryFailureAuditMessagesInBlockValidationMode)
{
    std::vector<uint160> recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
    ASSERT_THAT(recovery_message_hashes.size(), Eq(4));
    auto failure_message = relay_message_handler->succession_handler.GenerateSecretRecoveryFailureMessage(recovery_message_hashes);
    relay->hashes.recovery_failure_audit_message_hashes = vector<uint160>();
    relay_message_handler->mode = BLOCK_VALIDATION;
    relay_message_handler->HandleSecretRecoveryFailureMessage(failure_message);
    vector<uint160> audit_message_hashes = relay->hashes.recovery_failure_audit_message_hashes;
    ASSERT_THAT(audit_message_hashes.size(), Eq(0));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage,
       SendsAuditMessagesWhichContainTheReceivingKeyQuartersForTheSharedSecretQuartersReferencedInTheFailureMessage)
{
    vector<uint160> audit_message_hashes = relay->hashes.recovery_failure_audit_message_hashes;
    RecoveryFailureAuditMessage audit_message = data->GetMessage(audit_message_hashes[0]);

    auto key_sharer = GetKeySharer();
    int64_t position_of_key_quarter_holder = PositionOfEntryInVector(relay->number,
                                                                     key_sharer->holders.key_quarter_holders);

    auto key_sixteenth_position = 4 * position_of_key_quarter_holder + failure_message.shared_secret_quarter_position;
    auto public_key_sixteenth = key_sharer->PublicKeySixteenths()[key_sixteenth_position];
    auto key_quarter_position = PositionOfEntryInVector(audit_message.quarter_holder_number,
                                                        relay->holders.key_quarter_holders);
    auto receiving_key_quarter = relay->GenerateRecipientPrivateKeyQuarter(public_key_sixteenth,
                                                                           (uint8_t) key_quarter_position, *data);
    ASSERT_THAT(audit_message.private_receiving_key_quarter, Eq(receiving_key_quarter.getuint256()));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage,
       SendsAuditMessagesWhosePrivateReceivingKeyQuartersMatchTheCorrespondingPublicReceivingKeyQuarters)
{
    vector<uint160> audit_message_hashes = relay->hashes.recovery_failure_audit_message_hashes;

    for (uint32_t i = 0; i < 4; i++)
    {
        RecoveryFailureAuditMessage audit_message = data->GetMessage(audit_message_hashes[i]);
        bool ok = audit_message.VerifyPrivateReceivingKeyQuarterMatchesPublicReceivingKeyQuarter(*data);
        ASSERT_THAT(ok, Eq(true)) << "failed at " << i;
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage,
       SendsAuditMessagesWhichRevealWhetherTheEncryptedKeyQuarterInTheSecretRecoveryMessageWasCorrect)
{
    vector<uint160> audit_message_hashes = relay->hashes.recovery_failure_audit_message_hashes;

    for (uint32_t i = 0; i < 4; i++)
    {
        RecoveryFailureAuditMessage audit_message = data->GetMessage(audit_message_hashes[i]);
        bool ok = audit_message.VerifyEncryptedSharedSecretQuarterInSecretRecoveryMessageWasCorrect(*data);
        if (i < 3)
            ASSERT_THAT(ok, Eq(true));
        else
            ASSERT_THAT(ok, Eq(false));
    }
}

class ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages :
        public ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter
{
public:
    uint160 failure_message_hash;
    SecretRecoveryFailureMessage failure_message;
    SecretRecoveryMessage secret_recovery_message;

    virtual void SetUp()
    {
        suppress_audit_messages = true;
        ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter::SetUp();
        failure_message_hash = relay->hashes.secret_recovery_failure_message_hashes[0];
        failure_message = data->GetMessage(failure_message_hash);

        vector<uint160> recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
        secret_recovery_message = data->GetMessage(recovery_message_hashes[0]);
    }

    Relay *GetKeySharer()
    {
        uint64_t key_sharer_number = NumberOfARelayWhoSharedAQuarterKeyWithChosenRelay();
        return relay_message_handler->relay_state.GetRelayByNumber(key_sharer_number);
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesOneOfWhichHasABadSharedSecretQuarter::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages,
       RecordsTheSecretRecoveryFailureMessageHash)
{
    ASSERT_THAT(relay->hashes.secret_recovery_failure_message_hashes[0], Eq(failure_message_hash));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages,
       HasProcessedNoAuditMessages)
{
    vector<uint160> audit_message_hashes = relay->hashes.recovery_failure_audit_message_hashes;
    ASSERT_THAT(audit_message_hashes.size(), Eq(0));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages,
       RejectsAnAuditMessageWithAnImpossibleQuarterHolderNumber)
{
    RecoveryFailureAuditMessage audit_message;
    audit_message.failure_message_hash = failure_message_hash;
    audit_message.quarter_holder_number = 1000;
    audit_message.Sign(*data);
    relay_message_handler->Handle(audit_message, &peer);
    ASSERT_TRUE(IsRejected(audit_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages,
       WritesObituariesForTheKeyQuarterHoldersWhoDontRespondInTime)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (auto quarter_holder : relay->QuarterHolders(*data))
        ASSERT_THAT(quarter_holder->hashes.obituary_hash, Ne(0));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages,
       EncodesDurationsWithoutResponsesFromTheKeyQuarterHoldersWhoDontRespondInTime)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (auto quarter_holder : relay->QuarterHolders(*data))
    {
        DurationWithoutResponseFromRelay duration;
        duration.message_hash = failure_message_hash;
        duration.relay_number = quarter_holder->number;
        ASSERT_TRUE(VectorContainsEntry(builder->accepted_messages, duration.GetHash160()));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages,
       RejectsARecoveryFailureAuditMessageWhichArrivesAfterADurationWithoutResponse)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    RecoveryFailureAuditMessage audit_message;
    audit_message.Populate(failure_message, secret_recovery_message, *data);
    audit_message.Sign(*data);
    ASSERT_FALSE(relay_message_handler->ValidateRecoveryFailureAuditMessage(audit_message));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages,
       FailsValidationOfADurationWithoutResponseAfterAnAuditMessageHasBeenProcessed)
{
    RecoveryFailureAuditMessage audit_message;
    audit_message.Populate(failure_message, secret_recovery_message, *data);
    audit_message.Sign(*data);

    relay_message_handler->Handle(audit_message, &peer);

    DurationWithoutResponseFromRelay duration;
    duration.message_hash = failure_message_hash;
    duration.relay_number = relay->holders.key_quarter_holders[0];

    ASSERT_FALSE(relay_message_handler->ValidateDurationWithoutResponseFromRelay(duration));
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages,
       SchedulesATaskToCheckWhichQuarterHoldersHaveRespondedToTheObituaries)
{
    relay_message_handler->succession_handler.send_secret_recovery_messages = false;
    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (auto quarter_holder_number : relay->holders.key_quarter_holders)
    {
        auto quarter_holder = relay_state->GetRelayByNumber(quarter_holder_number);
        ASSERT_TRUE(relay_message_handler->succession_handler.scheduler.TaskIsScheduledForTime(
                    "obituary", quarter_holder->hashes.obituary_hash, TEST_START_TIME + 2 * RESPONSE_WAIT_TIME));
    }
}

TEST_F(ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessageAndNoAuditMessages,
        DoesntScheduleATaskToCheckWhichQuarterHoldersHaveRespondedToTheObituariesInBlockValidationMode)
{
    relay_message_handler->mode = BLOCK_VALIDATION;
    SetMockTimeMicros(TEST_START_TIME + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    for (auto quarter_holder_number : relay->holders.key_quarter_holders)
    {
        auto quarter_holder = relay_state->GetRelayByNumber(quarter_holder_number);
        ASSERT_FALSE(relay_message_handler->succession_handler.scheduler.TaskIsScheduledForTime(
                     "obituary", quarter_holder->hashes.obituary_hash, TEST_START_TIME + 2 * RESPONSE_WAIT_TIME));
    }
}


class ARelayMessageHandlerWhichHasProcessedFourValidRecoveryFailureAuditMessages :
        public ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage
{
    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage::SetUp();
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedASecretRecoveryFailureMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedFourValidRecoveryFailureAuditMessages,
       GeneratesAnObituaryForTheQuarterHolderWhoSentABadSecretRecoveryMessage)
{
    vector<uint160> audit_message_hashes = relay->hashes.recovery_failure_audit_message_hashes;
    RecoveryFailureAuditMessage audit_message = data->GetMessage(audit_message_hashes[3]);
    Relay *quarter_holder = relay_message_handler->relay_state.GetRelayByNumber(audit_message.quarter_holder_number);

    ASSERT_THAT(quarter_holder->hashes.obituary_hash, Ne(0));
}


class ARelayMessageHandlerWhichHasProcessedABadSecretRecoveryFailureMessage :
        public ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage
{
public:
    uint160 failure_message_hash{0};
    SecretRecoveryFailureMessage bad_failure_message;

    virtual void SetUp()
    {
        ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage::SetUp();

        bad_failure_message = AFailureMessageWhichFalselyStatesTheSumOfSharedSecretQuarters();
        relay_message_handler->Handle(bad_failure_message, NULL);
        failure_message_hash = bad_failure_message.GetHash160();
    }

    SecretRecoveryFailureMessage AFailureMessageWhichFalselyStatesTheSumOfSharedSecretQuarters()
    {
        SecretRecoveryFailureMessage failure_message;
        failure_message.key_sharer_position = 0;
        failure_message.shared_secret_quarter_position = 1;
        failure_message.obituary_hash = relay->hashes.obituary_hash;
        std::vector<uint160> recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
        failure_message.recovery_message_hashes = recovery_message_hashes;
        // falsely state that the decrypted shared secret quarters sum to 4
        failure_message.sum_of_decrypted_shared_secret_quarters = Point(CBigNum(4));
        failure_message.Sign(*data);
        return failure_message;
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWhichHasProcessedFourSecretRecoveryMessagesButNoSuccessionCompletedMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWhichHasProcessedABadSecretRecoveryFailureMessage,
       GeneratesAnObituaryForTheSuccessorWhoSentTheBadFailureMessage)
{
    auto successor = relay_message_handler->relay_state.GetRelayByNumber(relay->SuccessorNumber(*data));
    ASSERT_THAT(successor->hashes.obituary_hash, Ne(0));
}


class ARelayMessageHandlerWithAGoodRecoveryFailureMessageWhichRevealsBadEncryptedSecretsInTheRelayJoinMessage :
        public ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret
{
public:
    virtual void SetUp()
    {
        ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret::SetUp();
        DECLARE_CACHED_DATA;
        if (reference_relay_state.relays.size() == 0)
        {
            key_sharer->public_key_set.key_points[14][0] += Point(CBigNum(1));
            relay_message_handler->Handle(goodbye_message, &peer);
            SAVE_CACHED_DATA;
        }
        else
        {
            LOAD_CACHED_DATA;
        }
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAGoodbyeMessageWithABadSecret::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWithAGoodRecoveryFailureMessageWhichRevealsBadEncryptedSecretsInTheRelayJoinMessage,
       SendsFourAuditMessages)
{
    ASSERT_THAT(relay->hashes.recovery_failure_audit_message_hashes.size(), Eq(4));
}

TEST_F(ARelayMessageHandlerWithAGoodRecoveryFailureMessageWhichRevealsBadEncryptedSecretsInTheRelayJoinMessage,
       GeneratesAnObituaryForTheMisbehavingKeySharer)
{
    ASSERT_THAT(key_sharer->hashes.obituary_hash, Ne(0));
}

TEST_F(ARelayMessageHandlerWithAGoodRecoveryFailureMessageWhichRevealsBadEncryptedSecretsInTheRelayJoinMessage,
       DoesntGenerateAnObituaryForTheSuccessorOrQuarterHolders)
{
    auto successor = relay_state->GetRelayByNumber(relay->SuccessorNumber(*data));
    ASSERT_THAT(successor->hashes.obituary_hash, Eq(0));
    for (auto quarter_holder : relay->QuarterHolders(*data))
        ASSERT_THAT(quarter_holder->hashes.obituary_hash, Eq(0));
}


class ARelayMessageHandlerWithManyRelaysWhoseKeyDistributionMessagesHaveBeenAccepted :
        public ARelayMessageHandlerWithAKeyDistributionMessage
{
public:
    vector<Point> key_parts_held_by_dead_relay;

    virtual void SetUp()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::SetUp();
        SetUpKeyDistributionMessagesUsingCache();

        GenerateAndProcessKeyDistributionMessages();
    }

    void SetUpKeyDistributionMessagesUsingCache()
    {
        static vector<Point> reference_key_parts_held_by_dead_relay;
        DECLARE_CACHED_DATA;

        if (reference_relay_state.relays.size() == 0)
        {
            GenerateAndProcessKeyDistributionMessages();
            reference_key_parts_held_by_dead_relay = key_parts_held_by_dead_relay;
            SAVE_CACHED_DATA;
        }
        else
        {
            LOAD_CACHED_DATA;
            key_parts_held_by_dead_relay = reference_key_parts_held_by_dead_relay;
        }
    }

    void GenerateAndProcessKeyDistributionMessages()
    {
        for (auto &relay : relay_state->relays)
        {
            if (relay.hashes.key_distribution_message_hash != 0)
                continue;
            if (not relay_state->AssignKeyPartHoldersToRelay(relay, relay.number))
                continue;
            auto key_distribution_message = relay.GenerateKeyDistributionMessage(*data, relay.number, *relay_state);
            data->StoreMessage(key_distribution_message);
            relay_state->ProcessKeyDistributionMessage(key_distribution_message);
            relay.key_distribution_message_accepted = true;
        }
    }

    void DeleteKeyPartsHeldByRelay(uint64_t relay_number)
    {
        for (auto key_sixteenth : KeyPartsHeldByRelay(relay_number))
            keydata.Delete(key_sixteenth);
    }

    vector<Point> KeyPartsHeldByRelay(uint64_t relay_number)
    {
        if (key_parts_held_by_dead_relay.size() > 0)
            return key_parts_held_by_dead_relay;
        vector<Point> parts;
        auto specified_relay = relay_state->GetRelayByNumber(relay_number);
        for (auto sharer_number : KeySharersOfGivenRelay(relay_number))
        {
            auto sharer = relay_state->GetRelayByNumber(sharer_number);
            auto key_quarter_position = PositionOfEntryInVector(relay_number, sharer->holders.key_quarter_holders);
            for (int64_t i = 4 * key_quarter_position; i < 4 + 4 * key_quarter_position; i++)
                parts.push_back(sharer->PublicKeySixteenths()[i]);
        }
        key_parts_held_by_dead_relay = parts;
        return parts;
    }

    vector<uint64_t> KeySharersOfGivenRelay(uint64_t relay_number)
    {
        vector<uint64_t> sharers;
        for (auto &relay : relay_state->relays)
        {
            if (not relay.key_distribution_message_accepted) continue;
            if (VectorContainsEntry(relay.holders.key_quarter_holders, relay_number))
                sharers.push_back(relay.number);
        }
        return sharers;
    }

    vector<uint64_t> KeyPartHolders()
    {
        vector<uint64_t> holders;
        for (auto &relay : relay_state->relays)
            for (auto quarter_holder_number : relay.holders.key_quarter_holders)
                if (not VectorContainsEntry(holders, quarter_holder_number))
                    holders.push_back(quarter_holder_number);
        return holders;
    }

    void KillRelaySilently(uint64_t relay_number)
    {
        keydata.Delete(relay_state->GetRelayByNumber(relay_number)->public_signing_key);
    }

    void RegisterRelayDeath(uint64_t relay_number)
    {
        relay_state->RecordRelayDeath(relay_state->GetRelayByNumber(relay_number), *data, NOT_RESPONDING);
    }

    bool KeyPartsHeldByRelayArePresent(uint64_t relay_number)
    {
        for (auto public_key_sixteenth : KeyPartsHeldByRelay(relay_number))
            if (not keydata[public_key_sixteenth].HasProperty("privkey"))
                return false;
        return true;
    }

    virtual void TearDown()
    {
        ARelayMessageHandlerWithAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayMessageHandlerWithManyRelaysWhoseKeyDistributionMessagesHaveBeenAccepted,
       RecoversSecretsAfterTheDeathOfOneRelay)
{
    auto relay_number = KeyPartHolders()[0];
    DeleteKeyPartsHeldByRelay(relay_number);
    KillRelaySilently(relay_number);
    RegisterRelayDeath(relay_number);
    ASSERT_FALSE(KeyPartsHeldByRelayArePresent(relay_number));
    relay_message_handler->HandleNewlyDeadRelays();
    ASSERT_TRUE(KeyPartsHeldByRelayArePresent(relay_number));
}

TEST_F(ARelayMessageHandlerWithManyRelaysWhoseKeyDistributionMessagesHaveBeenAccepted,
       RecoversSecretsAfterTheDeathOfTwoRelaysOneOfWhichIsAQuarterHolderForTheOther)
{
    auto relay_number = KeyPartHolders()[0];
    auto quarter_holder_number = relay_state->GetRelayByNumber(relay_number)->holders.key_quarter_holders[0];

    DeleteKeyPartsHeldByRelay(relay_number);
    KillRelaySilently(relay_number);
    KillRelaySilently(quarter_holder_number);
    ASSERT_FALSE(KeyPartsHeldByRelayArePresent(relay_number));

    RegisterRelayDeath(relay_number);
    relay_message_handler->HandleNewlyDeadRelays();
    ASSERT_FALSE(KeyPartsHeldByRelayArePresent(relay_number));

    SetMockTimeMicros(GetTimeMicros() + RESPONSE_WAIT_TIME);
    relay_message_handler->succession_handler.scheduler.DoTasksScheduledForExecutionBeforeNow();

    ASSERT_FALSE(relay_state->ContainsRelayWithNumber(quarter_holder_number));
    ASSERT_TRUE(KeyPartsHeldByRelayArePresent(relay_number));
}