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

        data = &relay_message_handler->data;

        SetLatestMinedCreditMessageBatchNumberTo(1);
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

    for (Point &public_key_part : second_join.public_key_sixteenths)
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
        SetMockTimeMicros(TEST_START_TIME);
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

TEST_F(ARelayMessageHandlerWhichHasAccepted37Relays, RejectsAKeyDistributionMessageWithTheWrongNumbersOfSecrets)
{
    Relay &relay = relay_message_handler->relay_state.relays[30];
    auto key_distribution_message = relay.GenerateKeyDistributionMessage(*data, 10, relay_message_handler->relay_state);
    key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders.push_back(1);

    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);

    bool rejected = msgdata[key_distribution_message.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ARelayMessageHandlerWhichHasAccepted37Relays,
       SchedulesATaskToEncodeADurationWithoutResponseAfterReceivingAValidKeyDistributionMessage)
{
    Relay &relay = relay_message_handler->relay_state.relays[30];
    auto key_distribution_message = relay.GenerateKeyDistributionMessage(*data, 10, relay_message_handler->relay_state);

    relay_message_handler->HandleKeyDistributionMessage(key_distribution_message);

    ASSERT_TRUE(relay_message_handler->scheduler.TaskIsScheduledForTime("key_distribution",
                                                                        key_distribution_message.GetHash160(),
                                                                        TEST_START_TIME + RESPONSE_WAIT_TIME));
}