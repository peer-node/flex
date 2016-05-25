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
        tx.rawtx.outputs.push_back(Credit(Point(2).getvch(), 1));
        credit_system->StoreTransaction(tx);
        msg.hash_list.full_hashes.push_back(tx.GetHash160());
        msg.hash_list.GenerateShortHashes();
        CreditBatch batch;
        batch.Add(tx.rawtx.outputs[0]);
        msg.mined_credit.network_state.batch_root = batch.Root();
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