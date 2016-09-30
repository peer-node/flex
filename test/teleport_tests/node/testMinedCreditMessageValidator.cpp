#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "CreditSystem.h"
#include "MinedCreditMessageValidator.h"

using namespace ::testing;
using namespace std;


class AMinedCreditMessageValidator : public Test
{
public:
    MemoryDataStore msgdata, creditdata;
    MinedCreditMessageValidator validator;
    CreditSystem *credit_system;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        validator.SetCreditSystem(credit_system);
    }

    virtual void TearDown()
    {
        delete credit_system;
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
        tx.rawtx.outputs.push_back(Credit(Point(SECP256K1, 2).getvch(), 1));
        credit_system->StoreTransaction(tx);
        msg.hash_list.full_hashes.push_back(tx.GetHash160());
        msg.hash_list.GenerateShortHashes();
        CreditBatch batch;
        batch.Add(tx.rawtx.outputs[0]);
        msg.mined_credit.network_state.batch_number = 1;
        msg.mined_credit.network_state.batch_root = batch.Root();
        msg.mined_credit.network_state.difficulty = credit_system->initial_difficulty;
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