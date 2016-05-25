#include <test/flex_tests/mined_credits/MinedCredit.h>
#include "NetworkState.h"
#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include <src/credits/SignedTransaction.h>
#include "gmock/gmock.h"
#include "CreditSystem.h"
#include "BatchChainer.h"
#include "MinedCreditMessageValidator.h"

using namespace ::testing;
using namespace std;


class ABatchChainer : public Test
{
public:
    BatchChainer batch_chainer;
    MemoryDataStore msgdata, creditdata;
    CreditSystem *credit_system;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        batch_chainer.SetCreditSystem(credit_system);
    }

    virtual void TearDown()
    {
        delete credit_system;
    }
};

TEST_F(ABatchChainer, StartsWithALatestMinedCreditHashOfZero)
{
    uint160 latest_mined_credit_hash = batch_chainer.LatestMinedCreditHash();
    ASSERT_THAT(latest_mined_credit_hash, Eq(0));
}


class ABatchChainerWithBatchesToChain : public ABatchChainer
{
public:
    MinedCreditMessage SucceedingMinedCreditMessage(MinedCreditMessage& msg)
    {
        MinedCreditMessage next_msg;
        EncodedNetworkState state, previous_state = msg.mined_credit.network_state;

        state.network_id = previous_state.network_id;
        state.previous_mined_credit_hash = msg.mined_credit.GetHash160();
        state.batch_offset = previous_state.batch_offset + previous_state.batch_size;
        state.batch_number = previous_state.batch_number + 1;

        CreditBatch batch(state.previous_mined_credit_hash, state.batch_offset);
        batch.Add(msg.mined_credit);
        state.batch_root = batch.Root();
        state.batch_size = 1;

        auto spent_chain = credit_system->GetSpentChain(state.previous_mined_credit_hash);
        spent_chain.Add();
        state.spent_chain_hash = spent_chain.GetHash160();

        next_msg.mined_credit.network_state = state;

        next_msg.hash_list.full_hashes.push_back(msg.mined_credit.GetHash160());
        next_msg.hash_list.GenerateShortHashes();

        return next_msg;
    }

    virtual void SetUp()
    {
        ABatchChainer::SetUp();
    }

    virtual void TearDown()
    {
        ABatchChainer::TearDown();
    }
};

TEST_F(ABatchChainerWithBatchesToChain, AddsABatchToTheTip)
{
    MinedCreditMessage empty_msg;
    auto msg = SucceedingMinedCreditMessage(empty_msg);
    batch_chainer.AddBatchToTip(msg);
    ASSERT_THAT(batch_chainer.LatestMinedCreditHash(), Eq(msg.mined_credit.GetHash160()));
}

TEST_F(ABatchChainerWithBatchesToChain, RemovesABatchFromTheTip)
{
    MinedCreditMessage empty_msg;
    auto msg = SucceedingMinedCreditMessage(empty_msg);
    batch_chainer.AddBatchToTip(msg);
    auto msg2 = SucceedingMinedCreditMessage(msg);
    batch_chainer.AddBatchToTip(msg2);
    ASSERT_THAT(batch_chainer.LatestMinedCreditHash(), Eq(msg2.mined_credit.GetHash160()));
    batch_chainer.RemoveBatchFromTip();
    ASSERT_THAT(batch_chainer.LatestMinedCreditHash(), Eq(msg.mined_credit.GetHash160()));
}

class ABatchChainerWithAMinedCreditMessageValidator : public ABatchChainerWithBatchesToChain
{
public:
    MinedCreditMessageValidator validator;

    virtual void SetUp()
    {
        ABatchChainerWithBatchesToChain::SetUp();
        validator.SetCreditSystem(credit_system);
        batch_chainer.SetMinedCreditMessageValidator(&validator);
    }

    virtual void TearDown()
    {
        ABatchChainerWithBatchesToChain::TearDown();
    }
};

TEST_F(ABatchChainerWithAMinedCreditMessageValidator, ValidatesMinedCreditMessages)
{
    MinedCreditMessage empty_msg;
    auto msg = SucceedingMinedCreditMessage(empty_msg);
    bool ok = batch_chainer.ValidateMinedCreditMessage(msg);
    ASSERT_THAT(ok, Eq(true));
}