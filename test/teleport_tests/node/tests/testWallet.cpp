#include <src/credits/creditsign.h>
#include <test/teleport_tests/mining/MiningHashTree.h>
#include <src/credits/CreditBatch.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/wallet/Wallet.h"
#include "test/teleport_tests/node/credit/messages/MinedCreditMessage.h"
#include "test/teleport_tests/node/credit/structures/CreditSystem.h"

using namespace ::testing;
using namespace std;


class AWallet : public Test
{
public:
    MemoryDataStore keydata;
    Wallet *wallet;

    virtual void SetUp()
    {
        wallet = new Wallet(keydata);
    }

    virtual void TearDown()
    {
        delete wallet;
    }

    CreditInBatch CreditInBatchWhosePrivateKeyIsKnown()
    {
        Point public_key = wallet->GetNewPublicKey();
        Credit credit(public_key.getvch(), ONE_CREDIT);
        CreditBatch batch;
        batch.Add(credit);
        return batch.GetCreditInBatch(credit);
    }
};

TEST_F(AWallet, HasNoCreditsInitially)
{
    vector<CreditInBatch> credits_in_wallet = wallet->GetCredits();
    ASSERT_THAT(credits_in_wallet.size(), Eq(0));
}

TEST_F(AWallet, GeneratesPublicKeysWhosePrivateKeysAreKnown)
{
    Point public_key = wallet->GetNewPublicKey();
    ASSERT_TRUE(keydata[public_key].HasProperty("privkey"));
}

TEST_F(AWallet, AddsACreditInBatchIfThePrivateKeyIsKnown)
{
    auto credit_in_batch = CreditInBatchWhosePrivateKeyIsKnown();
    wallet->HandleCreditInBatch(credit_in_batch);
    ASSERT_THAT(VectorContainsEntry(wallet->GetCredits(), credit_in_batch), Eq(true));
}

class AWalletWithCredits : public AWallet
{
public:
    virtual void SetUp()
    {
        AWallet::SetUp();
        for (int i = 0; i < 5; i++)
            wallet->HandleCreditInBatch(CreditInBatchWhosePrivateKeyIsKnown());
    }
};

TEST_F(AWalletWithCredits, ReportsANonZeroBalance)
{
    uint64_t balance = wallet->Balance();
    ASSERT_THAT(balance, Eq(5 * ONE_CREDIT));
}

TEST_F(AWalletWithCredits, GeneratesASignedTransactionPayingToASpecifiedPublicKey)
{
    Point public_key(SECP256K1, 4);
    SignedTransaction tx = wallet->GetSignedTransaction(public_key, 2 * ONE_CREDIT);
    ASSERT_THAT(tx.TotalInputAmount(), Ge(2 * ONE_CREDIT));
    ASSERT_THAT(tx.rawtx.outputs[0].public_key, Eq(public_key));
}

TEST_F(AWalletWithCredits, GeneratesASignedTransactionPayingANonIntegerAmountToASpecifiedPublicKey)
{
    Point public_key(SECP256K1, 4);
    SignedTransaction tx = wallet->GetSignedTransaction(public_key, (uint64_t) (1.5 * ONE_CREDIT));
    ASSERT_THAT(tx.TotalInputAmount(), Ge(1.5 * ONE_CREDIT));
    ASSERT_THAT(uint160(tx.rawtx.outputs[0].public_key), Eq(public_key));
}

class AWalletWithCreditsAndAnIncomingMinedCreditMessage : public AWalletWithCredits
{
public:
    MemoryDataStore msgdata, creditdata, keydata, depositdata;
    Data *data{NULL};
    CreditSystem *credit_system;
    SignedTransaction tx;
    MinedCreditMessage msg_containing_tx;

    virtual void SetUp()
    {
        AWalletWithCredits::SetUp();
        data = new Data(msgdata, creditdata, keydata, depositdata);
        credit_system = new CreditSystem(*data);
        Point public_key = wallet->GetNewPublicKey();
        tx = wallet->GetSignedTransaction(public_key, 2 * ONE_CREDIT);
        credit_system->StoreTransaction(tx);
        msg_containing_tx = MinedCreditMessageContainingTransaction(tx);
    }

    MinedCreditMessage MinedCreditMessageContainingTransaction(SignedTransaction tx)
    {
        MinedCreditMessage msg;
        msg.hash_list.full_hashes.push_back(tx.GetHash160());
        msg.hash_list.GenerateShortHashes();
        return msg;
    }

    virtual void TearDown()
    {
        AWalletWithCredits::TearDown();
        delete credit_system;
        delete data;
    }
};

TEST_F(AWalletWithCreditsAndAnIncomingMinedCreditMessage, RemovesSpentCredits)
{
    ASSERT_THAT(VectorContainsEntry(wallet->credits, tx.rawtx.inputs[0]), Eq(true));
    wallet->AddBatchToTip(msg_containing_tx, credit_system);
    ASSERT_THAT(VectorContainsEntry(wallet->credits, tx.rawtx.inputs[0]), Eq(false));
}

TEST_F(AWalletWithCreditsAndAnIncomingMinedCreditMessage, AddsIncomingCredits)
{
    wallet->AddBatchToTip(msg_containing_tx, credit_system);
    ASSERT_THAT(wallet->Balance(), Eq(5 * ONE_CREDIT));
}

TEST_F(AWalletWithCreditsAndAnIncomingMinedCreditMessage, SortsCredits)
{
    wallet->AddBatchToTip(msg_containing_tx, credit_system);
    ASSERT_THAT(wallet->credits.back().amount, Eq(2 * ONE_CREDIT));
}

class AWalletWhichHasHandledAMinedCreditMessage : public AWalletWithCreditsAndAnIncomingMinedCreditMessage
{
public:
    virtual void SetUp()
    {
        AWalletWithCreditsAndAnIncomingMinedCreditMessage::SetUp();
        wallet->AddBatchToTip(msg_containing_tx, credit_system);
    }
};

TEST_F(AWalletWhichHasHandledAMinedCreditMessage, ReaddsSpentCreditsWhenRemovingABatchFromTheTip)
{
    ASSERT_THAT(VectorContainsEntry(wallet->credits, tx.rawtx.inputs[0]), Eq(false));
    wallet->RemoveBatchFromTip(msg_containing_tx, credit_system);
    ASSERT_THAT(VectorContainsEntry(wallet->credits, tx.rawtx.inputs[0]), Eq(true));
}

TEST_F(AWalletWhichHasHandledAMinedCreditMessage, RemovesEnclosedCreditsWhenRemovingABatchFromTheTip)
{
    ASSERT_THAT(wallet->Balance(), Eq(5 * ONE_CREDIT));
    wallet->RemoveBatchFromTip(msg_containing_tx, credit_system);
    ASSERT_THAT(wallet->Balance(), Eq(5 * ONE_CREDIT));
}