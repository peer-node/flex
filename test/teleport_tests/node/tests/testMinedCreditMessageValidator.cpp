#include <src/base/util_time.h>
#include <test/teleport_tests/node/relays/structures/RelayState.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/credit/structures/CreditSystem.h"
#include "test/teleport_tests/node/credit/handlers/MinedCreditMessageValidator.h"
#include "test/teleport_tests/node/relays/structures/RelayMemoryCache.h"


#include "log.h"
#define LOG_CATEGORY "test"

using namespace ::testing;
using namespace std;


class AMinedCreditMessageValidator : public Test
{
public:
    MemoryDataStore msgdata, creditdata, keydata, depositdata;
    MinedCreditMessageValidator validator;
    CreditSystem *credit_system;
    Data *data;

    virtual void SetUp()
    {
        data = new Data(msgdata, creditdata, keydata, depositdata);
        credit_system = new CreditSystem(*data);

        data->cache = new RelayMemoryCache();
        validator.SetCreditSystem(credit_system);
        validator.SetData(data);
    }

    virtual void TearDown()
    {
        delete credit_system;
        delete data;
    }

    MinedCreditMessage MinedCreditMessageWithAHash()
    {
        MinedCreditMessage mined_credit_message;
        mined_credit_message.hash_list.full_hashes.push_back(1);
        mined_credit_message.hash_list.GenerateShortHashes();
        return mined_credit_message;
    }

    MinedCreditMessage MinedCreditMessageWithABatch()
    {
        MinedCreditMessage msg;
        SignedTransaction tx;
        tx.rawtx.outputs.push_back(Credit(Point(SECP256K1, 2), 1));
        keydata[Point(SECP256K1, 2)]["privkey"] = CBigNum(2);
        credit_system->StoreTransaction(tx);
        msg.hash_list.full_hashes.push_back(tx.GetHash160());
        msg.hash_list.GenerateShortHashes();
        CreditBatch batch;
        batch.Add(tx.rawtx.outputs[0]);
        msg.mined_credit.network_state.batch_number = 1;
        msg.mined_credit.network_state.batch_root = batch.Root();
        msg.mined_credit.network_state.difficulty = credit_system->initial_difficulty;
        msg.mined_credit.public_key = Point(SECP256K1, 2);
        return msg;
    }

    MinedCreditMessage MinedCreditMessageWithARelayJoinMessage()
    {
        auto first_msg = MinedCreditMessageWithABatch();
        credit_system->StoreMinedCreditMessage(first_msg);
        credit_system->AddToMainChain(first_msg);
        MinedCreditMessage second_msg;
        second_msg.mined_credit.network_state = credit_system->SucceedingNetworkState(first_msg);
        RelayState relay_state;
        msgdata[first_msg.GetHash160()]["msg"] = first_msg;
        auto join = JoinMessageReferencingMinedCreditMessage(first_msg.GetHash160());
        second_msg.hash_list.full_hashes.push_back(join.GetHash160());
        second_msg.hash_list.GenerateShortHashes();
        relay_state.ProcessRelayJoinMessage(join);
        second_msg.mined_credit.network_state.relay_state_hash = relay_state.GetHash160();
        data->cache->Store(relay_state);
        return second_msg;
    }

    RelayJoinMessage JoinMessageReferencingMinedCreditMessage(uint160 msg_hash)
    {
        RelayJoinMessage join = Relay().GenerateJoinMessage(keydata, msg_hash);
        join.Sign(*data);
        data->StoreMessage(join);
        return join;
    }

    MinedCreditMessage MinedCreditMessageWithAValidAndAnInvalidRelayJoinMessage()
    {
        auto first_msg = MinedCreditMessageWithABatch();
        credit_system->StoreMinedCreditMessage(first_msg);
        credit_system->AddToMainChain(first_msg);
        msgdata[first_msg.GetHash160()]["msg"] = first_msg;

        return MinedCreditMessageContainingTwoJoinMessagesWithTheSameMinedCreditMessageHash(first_msg);
    }

    MinedCreditMessage MinedCreditMessageContainingTwoJoinMessagesWithTheSameMinedCreditMessageHash(
            MinedCreditMessage previous_msg)
    {
        MinedCreditMessage msg;
        msg.mined_credit.network_state = credit_system->SucceedingNetworkState(previous_msg);
        auto join1 = JoinMessageReferencingMinedCreditMessage(previous_msg.GetHash160());
        auto join2 = JoinMessageReferencingMinedCreditMessage(previous_msg.GetHash160());
        msg.hash_list.full_hashes.push_back(join1.GetHash160());
        msg.hash_list.full_hashes.push_back(join2.GetHash160());
        msg.hash_list.GenerateShortHashes();

        RelayState relay_state;
        relay_state.ProcessRelayJoinMessage(join1);
        msg.mined_credit.network_state.relay_state_hash = relay_state.GetHash160();

        return msg;
    }
};

TEST_F(AMinedCreditMessageValidator, DetectsWhenDataIsMissing)
{
    auto mined_credit_message = MinedCreditMessageWithAHash();
    bool data_is_missing = validator.HasMissingData(mined_credit_message);
    ASSERT_TRUE(data_is_missing);
}

TEST_F(AMinedCreditMessageValidator, DetectsWhenDataIsNotMissing)
{
    auto msg = MinedCreditMessageWithAHash();
    credit_system->StoreHash(msg.hash_list.full_hashes[0]);
    bool data_is_missing = validator.HasMissingData(msg);
    ASSERT_FALSE(data_is_missing);
}

TEST_F(AMinedCreditMessageValidator, DetectsWhenTheBatchRootIsCorrect)
{
    auto msg = MinedCreditMessageWithABatch();
    bool ok = validator.CheckBatchRoot(msg);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(AMinedCreditMessageValidator, DetectsWhenTheBatchRootIsIncorrect)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.batch_root = 0;
    bool ok = validator.CheckBatchRoot(msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheCreditBatchSize)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.batch_size = 0;
    bool ok = validator.CheckBatchSize(msg);
    ASSERT_THAT(ok, Eq(false));
    msg.mined_credit.network_state.batch_size = 1;
    ok = validator.CheckBatchSize(msg);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheCreditBatchOffset)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.batch_offset = 0;
    bool ok = validator.CheckBatchOffset(msg);
    ASSERT_THAT(ok, Eq(true));
    msg.mined_credit.network_state.batch_offset = 1;
    ok = validator.CheckBatchOffset(msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheCreditBatchNumber)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.batch_number = 1;
    bool ok = validator.CheckBatchNumber(msg);
    ASSERT_THAT(ok, Eq(true));
    msg.mined_credit.network_state.batch_number = 0;
    ok = validator.CheckBatchNumber(msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheMessageListHash)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.message_list_hash = msg.hash_list.GetHash160();
    bool ok = validator.CheckMessageListHash(msg);
    ASSERT_THAT(ok, Eq(true));
    msg.mined_credit.network_state.message_list_hash = msg.hash_list.GetHash160() + 1;
    ok = validator.CheckMessageListHash(msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheSpentChainHash)
{
    auto msg = MinedCreditMessageWithABatch();
    BitChain spent_chain;
    spent_chain.Add();
    msg.mined_credit.network_state.spent_chain_hash = spent_chain.GetHash160();
    msg.mined_credit.network_state.batch_size = 1;
    credit_system->StoreMinedCreditMessage(msg);
    bool ok = validator.CheckSpentChainHash(msg);
    ASSERT_THAT(ok, Eq(true));
    msg.mined_credit.network_state.spent_chain_hash = spent_chain.GetHash160() + 1;
    ok = validator.CheckSpentChainHash(msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheTimeStampSucceedsThePreviousTimestamp)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.timestamp = (uint64_t) GetAdjustedTimeMicros();
    credit_system->StoreMinedCreditMessage(msg);

    MinedCreditMessage next_msg;
    next_msg.mined_credit.network_state = credit_system->SucceedingNetworkState(msg);
    next_msg.mined_credit.network_state.timestamp = (uint64_t) (GetAdjustedTimeMicros() - 1e15);
    bool ok = validator.CheckTimeStampSucceedsPreviousTimestamp(next_msg);
    ASSERT_THAT(ok, Eq(false));
    next_msg.mined_credit.network_state.timestamp = (uint64_t) (GetAdjustedTimeMicros() + 1e6);
    ok = validator.CheckTimeStampSucceedsPreviousTimestamp(next_msg);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheTimeStampIsNotInTheFutureWithTwoSecondsLeeway)
{
    SetTimeOffset(100);
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.timestamp = (uint64_t) (GetAdjustedTimeMicros() + 2.5e6);
    credit_system->StoreMinedCreditMessage(msg);

    bool ok = validator.CheckTimeStampIsNotInFuture(msg);
    ASSERT_THAT(ok, Eq(false));
    msg.mined_credit.network_state.timestamp = (uint64_t) (GetAdjustedTimeMicros() + 1.5e6);
    ok = validator.CheckTimeStampIsNotInFuture(msg);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(AMinedCreditMessageValidator, ChecksThePreviousTotalWork)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.previous_total_work = 10;
    msg.mined_credit.network_state.difficulty = 2;
    credit_system->StoreMinedCreditMessage(msg);

    MinedCreditMessage next_msg;
    next_msg.mined_credit.network_state.previous_mined_credit_message_hash = msg.GetHash160();
    next_msg.mined_credit.network_state.previous_total_work = 11;
    bool ok = validator.CheckPreviousTotalWork(next_msg);
    ASSERT_THAT(ok, Eq(false));
    next_msg.mined_credit.network_state.previous_total_work = 12;
    ok = validator.CheckPreviousTotalWork(next_msg);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheDifficulty)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.difficulty = credit_system->initial_difficulty;
    credit_system->StoreMinedCreditMessage(msg);

    MinedCreditMessage next_msg;
    next_msg.mined_credit.network_state.previous_mined_credit_message_hash = msg.GetHash160();

    next_msg.mined_credit.network_state.difficulty = credit_system->GetNextDifficulty(msg);
    bool ok = validator.CheckDifficulty(next_msg);
    ASSERT_THAT(ok, Eq(true));
    next_msg.mined_credit.network_state.difficulty += 1;
    ok = validator.CheckDifficulty(next_msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheDiurnalDifficulty)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.diurnal_difficulty = INITIAL_DIURNAL_DIFFICULTY;
    credit_system->StoreMinedCreditMessage(msg);

    MinedCreditMessage next_msg;
    next_msg.mined_credit.network_state.previous_mined_credit_message_hash = msg.GetHash160();

    next_msg.mined_credit.network_state.diurnal_difficulty = INITIAL_DIURNAL_DIFFICULTY;
    bool ok = validator.CheckDiurnalDifficulty(next_msg);
    ASSERT_THAT(ok, Eq(true));
    next_msg.mined_credit.network_state.diurnal_difficulty = INITIAL_DIURNAL_DIFFICULTY + 1;
    ok = validator.CheckDiurnalDifficulty(next_msg);
    ASSERT_THAT(ok, Eq(false));
    creditdata[msg.mined_credit.GetHash160()]["is_calend"] = true;
    ok = validator.CheckDiurnalDifficulty(next_msg);
    ASSERT_THAT(ok, Eq(false));
    next_msg.mined_credit.network_state.diurnal_difficulty = credit_system->GetNextDiurnalDifficulty(msg);
    ok = validator.CheckDiurnalDifficulty(next_msg);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(AMinedCreditMessageValidator, ChecksThePreviousDiurnRoot)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.previous_diurn_root = 1;
    credit_system->StoreMinedCreditMessage(msg);

    MinedCreditMessage next_msg;
    next_msg.mined_credit.network_state.previous_mined_credit_message_hash = msg.GetHash160();
    next_msg.mined_credit.network_state.previous_diurn_root = 1;

    bool ok = validator.CheckPreviousDiurnRoot(next_msg);
    ASSERT_THAT(ok, Eq(true));
    next_msg.mined_credit.network_state.previous_diurn_root = 2;
    ok = validator.CheckPreviousDiurnRoot(next_msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(AMinedCreditMessageValidator, ChecksThePreviousCalendHash)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.previous_calend_hash= 1;
    credit_system->StoreMinedCreditMessage(msg);

    MinedCreditMessage next_msg;
    next_msg.mined_credit.network_state.previous_mined_credit_message_hash = msg.GetHash160();
    next_msg.mined_credit.network_state.previous_calend_hash = 1;

    bool ok = validator.CheckPreviousCalendHash(next_msg);
    ASSERT_THAT(ok, Eq(true));
    next_msg.mined_credit.network_state.previous_calend_hash = 2;
    ok = validator.CheckPreviousCalendHash(next_msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(AMinedCreditMessageValidator, ChecksTheDiurnalBlockRoot)
{
    auto msg = MinedCreditMessageWithABatch();
    msg.mined_credit.network_state.previous_diurn_root = 1;
    credit_system->StoreMinedCreditMessage(msg);

    MinedCreditMessage next_msg;
    next_msg.mined_credit.network_state.previous_mined_credit_message_hash = msg.GetHash160();
    next_msg.mined_credit.network_state.diurnal_block_root = credit_system->GetNextDiurnalBlockRoot(msg);

    bool ok = validator.CheckDiurnalBlockRoot(next_msg);
    ASSERT_THAT(ok, Eq(true));
    next_msg.mined_credit.network_state.diurnal_block_root = 2;
    ok = validator.CheckDiurnalBlockRoot(next_msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(AMinedCreditMessageValidator, AcceptsTheRelayStateHashWhenTheMinedCreditMessageContainsAValidJoinMessage)
{
    auto msg = MinedCreditMessageWithARelayJoinMessage();
    credit_system->StoreMinedCreditMessage(msg);
    bool ok = validator.CheckRelayStateHash(msg);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(AMinedCreditMessageValidator, RejectsTheRelayStateHashWhenTheMinedCreditMessageContainsAnInvalidJoinMessage)
{
    auto msg = MinedCreditMessageWithAValidAndAnInvalidRelayJoinMessage();
    credit_system->StoreMinedCreditMessage(msg);

    bool ok = validator.CheckRelayStateHash(msg);
    ASSERT_THAT(ok, Eq(false));
}


class AMinedCreditMessageValidatorWithAnEncodedKeyDistributionMessage : public AMinedCreditMessageValidator
{
public:
    virtual void SetUp()
    {
        AMinedCreditMessageValidator::SetUp();
    }

    virtual void TearDown()
    {
        AMinedCreditMessageValidator::TearDown();
    }

    RelayState RelayStateWith37Relays()
    {
        RelayState state;

        for (uint32_t i = 0; i < 37; i++)
        {
            auto join = Relay().GenerateJoinMessage(keydata, i);
            data->StoreMessage(join);
            state.ProcessRelayJoinMessage(join);
        }

        return state;
    }

    MinedCreditMessage MinedCreditMessageContainingARelayStateWith37Relays()
    {
        auto state = RelayStateWith37Relays();
        data->cache->Store(state);
        MinedCreditMessage msg;
        msg.mined_credit.network_state.relay_state_hash = state.GetHash160();
        msg.mined_credit.network_state.batch_number = 1;
        return msg;
    }

    MinedCreditMessage MinedCreditMessageContainingAKeyDistributionMessage()
    {
        auto msg = MinedCreditMessageContainingARelayStateWith37Relays();
        credit_system->StoreMinedCreditMessage(msg);
        data->StoreMessage(msg);

        auto state = data->cache->RetrieveRelayState(msg.mined_credit.network_state.relay_state_hash);
        auto &relay = state.relays[0];
        auto key_distribution_message = GenerateKeyDistributionMessage(state, msg.GetHash160());

        state.ProcessKeyDistributionMessage(key_distribution_message);
        data->cache->Store(state);

        return MinedCreditMessageContainingKeyDistributionMessage(msg, key_distribution_message, state);
    }

    KeyDistributionMessage GenerateKeyDistributionMessage(RelayState &state, uint160 mined_credit_message_hash)
    {
        auto &relay = state.relays[0];
        auto key_distribution_message = relay.GenerateKeyDistributionMessage(*data, mined_credit_message_hash, state);
        key_distribution_message.encrypted_key_quarters[0].encrypted_key_twentyfourths[0] += 1;
        key_distribution_message.Sign(*data);
        data->StoreMessage(key_distribution_message);
        return key_distribution_message;
    }

    template<typename T>
    MinedCreditMessage AddMessageToMinedCreditMessage(T message, MinedCreditMessage &msg, RelayState &state)
    {
        msg.mined_credit.network_state.relay_state_hash = state.GetHash160();
        msg.hash_list.RecoverFullHashes(data->msgdata);
        msg.hash_list.full_hashes.push_back(message.GetHash160());
        msg.hash_list.GenerateShortHashes();
        data->StoreMessage(msg);
        return msg;
    }

    MinedCreditMessage MinedCreditMessageContainingKeyDistributionMessage(
            MinedCreditMessage previous_msg,
            KeyDistributionMessage key_distribution_message,
            RelayState &state)
    {
        MinedCreditMessage next_msg;
        next_msg.mined_credit.network_state = credit_system->SucceedingNetworkState(previous_msg);
        next_msg.hash_list.full_hashes.push_back(previous_msg.GetHash160());
        next_msg.hash_list.GenerateShortHashes();
        AddMessageToMinedCreditMessage(key_distribution_message, next_msg, state);
        return next_msg;
    }

    MinedCreditMessage MinedCreditMessageContainingAKeyDistributionMessageAndADurationWithoutResponseAfterIt()
    {
        MinedCreditMessage msg = MinedCreditMessageContainingAKeyDistributionMessage();
        DurationWithoutResponse duration;
        duration.message_hash = msg.hash_list.full_hashes[1];
        data->StoreMessage(duration);
        msg.hash_list.full_hashes.push_back(duration.GetHash160());
        msg.hash_list.GenerateShortHashes();

        auto state = data->cache->RetrieveRelayState(msg.mined_credit.network_state.relay_state_hash);

        state.ProcessDurationWithoutResponse(duration, *data);
        data->StoreRelayState(&state);
        msg.mined_credit.network_state.relay_state_hash = state.GetHash160();
        data->StoreMessage(msg);
        return msg;
    }
};

TEST_F(AMinedCreditMessageValidatorWithAnEncodedKeyDistributionMessage,
       AcceptsTheRelayStateHashInTheMinedCreditMessageContainingTheKeyDistributionMessage)
{
    auto msg = MinedCreditMessageContainingAKeyDistributionMessage();
    credit_system->StoreMinedCreditMessage(msg);
    bool ok = validator.CheckRelayStateHash(msg);
    ASSERT_THAT(ok, Eq(true));
}

class AMinedCreditMessageValidatorWithAnEncodedKeyDistributionComplaint :
        public AMinedCreditMessageValidatorWithAnEncodedKeyDistributionMessage
{
public:
    KeyDistributionComplaint complaint;
    virtual void SetUp()
    {
        AMinedCreditMessageValidatorWithAnEncodedKeyDistributionMessage::SetUp();
    }

    MinedCreditMessage MinedCreditMessageWithAnEncodedKeyDistributionComplaint()
    {
        auto msg = MinedCreditMessageContainingAKeyDistributionMessage();

        auto state = data->cache->RetrieveRelayState(msg.mined_credit.network_state.relay_state_hash);
        auto complaint = GenerateKeyDistributionComplaint(state);

        data->relay_state = &state;
        state.ProcessKeyDistributionComplaint(complaint, *data);
        data->StoreRelayState(&state);
        AddMessageToMinedCreditMessage(complaint, msg, state);
        data->relay_state = NULL;
        return msg;
    }

    KeyDistributionComplaint GenerateKeyDistributionComplaint(RelayState &state)
    {
        KeyDistributionMessage key_distribution_message;
        auto key_distribution_message_hash = state.relays[0].key_quarter_locations[0].message_hash;
        key_distribution_message = msgdata[key_distribution_message_hash]["key_distribution"];
        complaint.Populate(key_distribution_message.GetHash160(), 0, Data(*data, &state));
        complaint.Sign(Data(*data, &state));
        data->StoreMessage(complaint);
        return complaint;
    }

    virtual void TearDown()
    {
        AMinedCreditMessageValidatorWithAnEncodedKeyDistributionMessage::TearDown();
    }
};

TEST_F(AMinedCreditMessageValidatorWithAnEncodedKeyDistributionComplaint,
       ChecksThatTheKeyDistributionMessageSenderHasBeenMarkedAsHavingMisbehaved)
{
    auto msg = MinedCreditMessageWithAnEncodedKeyDistributionComplaint();

    credit_system->StoreMinedCreditMessage(msg);
    bool ok = validator.CheckRelayStateHash(msg);
    ASSERT_THAT(ok, Eq(true));

    auto state = data->GetRelayState(msg.mined_credit.network_state.relay_state_hash);
    ASSERT_THAT(state.relays[0].status, Ne(MISBEHAVED));
}


class AMinedCreditMessageValidatorWithAnEncodedDurationWithoutResponseFromARelay :
        public AMinedCreditMessageValidatorWithAnEncodedKeyDistributionComplaint
{
public:
    virtual void SetUp()
    {
        AMinedCreditMessageValidatorWithAnEncodedKeyDistributionComplaint::SetUp();
    }

    MinedCreditMessage MinedCreditMessageWithAnEncodedDurationAfterAKeyDistributionComplaint()
    {
        auto msg = MinedCreditMessageWithAnEncodedKeyDistributionComplaint();

        auto state = data->cache->RetrieveRelayState(msg.mined_credit.network_state.relay_state_hash);
        data->relay_state = &state;

        DurationWithoutResponseFromRelay duration;
        duration.message_hash = Death(state.relays[0].number);
        duration.relay_number = state.relays[0].key_quarter_holders[0];

        data->StoreMessage(duration);
        state.ProcessDurationWithoutResponseFromRelay(duration, *data);
        data->StoreRelayState(&state);
        AddMessageToMinedCreditMessage(duration, msg, state);
        return msg;
    }

    virtual void TearDown()
    {
        AMinedCreditMessageValidatorWithAnEncodedKeyDistributionComplaint::TearDown();
    }
};

TEST_F(AMinedCreditMessageValidatorWithAnEncodedDurationWithoutResponseFromARelay,
       ChecksThatTheRelayIsMarkedAsNotResponding)
{
    auto msg = MinedCreditMessageWithAnEncodedDurationAfterAKeyDistributionComplaint();

    credit_system->StoreMinedCreditMessage(msg);
    bool ok = validator.CheckRelayStateHash(msg);
    ASSERT_THAT(ok, Eq(true));

    auto state = data->GetRelayState(msg.mined_credit.network_state.relay_state_hash);
    data->relay_state = &state;
    auto quarter_holder = state.relays[0].QuarterHolders(*data)[0];
    ASSERT_THAT(quarter_holder->status, Eq(NOT_RESPONDING));
}

class AMinedCreditMessageValidatorWithBothAnEncodedResponseAndDurationWithoutResponse :
        public AMinedCreditMessageValidatorWithAnEncodedDurationWithoutResponseFromARelay
{
public:
    virtual void SetUp()
    {
        AMinedCreditMessageValidatorWithAnEncodedDurationWithoutResponseFromARelay::SetUp();
    }

    MinedCreditMessage MinedCreditMessageWithAnEncodedResponseAndDurationWithoutResponse()
    {
        auto msg = MinedCreditMessageWithAnEncodedKeyDistributionComplaint();

        auto state = data->cache->RetrieveRelayState(msg.mined_credit.network_state.relay_state_hash);
        data->relay_state = &state;

        SecretRecoveryMessage recovery_message;
        recovery_message.dead_relay_number = state.relays[0].number;
        recovery_message.quarter_holder_number = state.relays[0].key_quarter_holders[0];
        recovery_message.successor_number = state.relays[0].SuccessorNumberFromLatestSuccessionAttempt(*data);
        recovery_message.PopulateSecrets(*data);
        recovery_message.Sign(*data);
        data->StoreMessage(recovery_message);

        AddMessageToMinedCreditMessage(recovery_message, msg, state);
        state.ProcessSecretRecoveryMessage(recovery_message);

        DurationWithoutResponseFromRelay duration;
        duration.message_hash = Death(state.relays[0].number);
        duration.relay_number = state.relays[0].key_quarter_holders[0];

        data->StoreMessage(duration);
        state.ProcessDurationWithoutResponseFromRelay(duration, *data);
        data->StoreRelayState(&state);
        AddMessageToMinedCreditMessage(duration, msg, state);
        return msg;
    }

    virtual void TearDown()
    {
        AMinedCreditMessageValidatorWithAnEncodedDurationWithoutResponseFromARelay::TearDown();
    }
};

TEST_F(AMinedCreditMessageValidatorWithBothAnEncodedResponseAndDurationWithoutResponse, FailsTheValidation)
{
    auto msg = MinedCreditMessageWithAnEncodedResponseAndDurationWithoutResponse();

    credit_system->StoreMinedCreditMessage(msg);
    bool ok = validator.CheckRelayStateHash(msg);
    ASSERT_THAT(ok, Eq(false));
}