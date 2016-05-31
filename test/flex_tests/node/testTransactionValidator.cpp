#include "gmock/gmock.h"
#include "CreditSystem.h"
#include "TransactionValidator.h"

using namespace ::testing;
using namespace std;

class ATransactionValidator : public Test
{
public:
    TransactionValidator transaction_validator;
    MemoryDataStore msgdata, creditdata;
    CreditSystem *credit_system;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        transaction_validator.SetCreditSystem(credit_system);
    }

    virtual void TearDown()
    {
        delete credit_system;
    }

    SignedTransaction ATransactionWhoseOutputsOverflow()
    {
        SignedTransaction tx;
        uint64_t amount1{0}, amount2{0};

        amount1 -= 1;
        amount2 = 2;

        vch_t key = Point(SECP256K1, 2).getvch();
        tx.rawtx.outputs.push_back(Credit(key, amount1));
        tx.rawtx.outputs.push_back(Credit(key, amount2));

        return tx;
    }

    CreditInBatch InputFromABatchInTheMainChain()
    {
        Credit credit(Point(SECP256K1, 2).getvch(), 1);
        SignedTransaction tx;
        tx.rawtx.outputs.push_back(credit);
        credit_system->StoreTransaction(tx);
        MinedCreditMessage msg;
        msg.hash_list.full_hashes.push_back(tx.GetHash160());
        msg.hash_list.GenerateShortHashes();
        CreditBatch batch = credit_system->ReconstructBatch(msg);
        msg.mined_credit.network_state.batch_root = batch.Root();
        msg.mined_credit.network_state.batch_size = 1;
        msg.mined_credit.network_state.difficulty = INITIAL_DIFFICULTY;
        credit_system->StoreMinedCreditMessage(msg);
        credit_system->AddToMainChain(msg);

        return batch.GetCreditInBatch(credit);
    }
};

TEST_F(ATransactionValidator, DetectsWhenTheSumOfTheOutputsOverflows)
{
    auto tx = ATransactionWhoseOutputsOverflow();
    bool overflow = transaction_validator.OutputsOverflow(tx);
    ASSERT_THAT(overflow, Eq(true));
}

TEST_F(ATransactionValidator, DetectsWhenTheSumOfTheOutputsDoesntOverflow)
{
    auto tx = ATransactionWhoseOutputsOverflow();
    tx.rawtx.outputs[0].amount = 5;
    bool overflow = transaction_validator.OutputsOverflow(tx);
    ASSERT_THAT(overflow, Eq(false));
}

TEST_F(ATransactionValidator, DetectsWhenATransactionInputIsFromABatchInTheMainChain)
{
    auto credit_in_batch = InputFromABatchInTheMainChain();
    bool in_main_chain = transaction_validator.CheckIfCreditInBatchIsInTheMainChain(credit_in_batch);
    ASSERT_THAT(in_main_chain, Eq(true));
}

TEST_F(ATransactionValidator, DetectsWhenATransactionInputIsntFromABatchInTheMainChain)
{
    auto credit_in_batch = InputFromABatchInTheMainChain();
    credit_in_batch.branch.back() += 1;
    bool in_main_chain = transaction_validator.CheckIfCreditInBatchIsInTheMainChain(credit_in_batch);
    ASSERT_THAT(in_main_chain, Eq(false));
}

TEST_F(ATransactionValidator, DetectsWhenTheBranchOfACreditInBatchIsValid)
{
    auto credit_in_batch = InputFromABatchInTheMainChain();
    bool branch_is_valid = transaction_validator.CheckBranchOfCreditInBatch(credit_in_batch);
    ASSERT_THAT(branch_is_valid, Eq(true));
}

TEST_F(ATransactionValidator, DetectsWhenTheBranchOfACreditInBatchIsNotValid)
{
    auto credit_in_batch = InputFromABatchInTheMainChain();
    credit_in_batch.branch.back() += 1;
    bool branch_is_valid = transaction_validator.CheckBranchOfCreditInBatch(credit_in_batch);
    ASSERT_THAT(branch_is_valid, Eq(false));
}