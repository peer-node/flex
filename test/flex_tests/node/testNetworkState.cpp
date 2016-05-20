#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <test/flex_tests/mined_credits/EncodedNetworkState.h>
#include "NetworkState.h"
#include <src/crypto/point.h>
#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include <src/credits/UnsignedTransaction.h>
#include <src/credits/SignedTransaction.h>
#include "gmock/gmock.h"

using namespace ::testing;
using namespace std;

class ANetworkState : public Test
{
public:
    NetworkState network_state;
};

TEST_F(ANetworkState, GeneratesAnEncodedNetworkState)
{
    EncodedNetworkState encoded_network_state = network_state.GetEncodedState();
}

TEST_F(ANetworkState, GeneratesABatchRoot)
{
    uint160 batch_root = network_state.GetBatchRoot();
}

SignedTransaction TransactionWithOneOutput()
{
    UnsignedTransaction raw_tx;
    Point pubkey(2);
    Credit credit(pubkey.getvch(), 100);
    raw_tx.outputs.push_back(credit);
    SignedTransaction tx;
    tx.rawtx = raw_tx;
    return tx;
}

TEST_F(ANetworkState, AddsTransactions)
{
    SignedTransaction tx = TransactionWithOneOutput();
    network_state.AddTransaction(tx);
    ASSERT_THAT(network_state.transactions.size(), Eq(1));
}

TEST_F(ANetworkState, AddsTheOutputsOfTransactionsToTheCreditBatch)
{
    SignedTransaction tx = TransactionWithOneOutput();
    network_state.AddTransactionOutputsToCreditBatch(tx);
    ASSERT_THAT(network_state.credit_batch.size(), Eq(1));
}

TEST_F(ANetworkState, ProvidesAValidBranchForCreditsInTheBatch)
{
    SignedTransaction tx = TransactionWithOneOutput();
    Credit credit = tx.rawtx.outputs[0];
    network_state.AddTransactionOutputsToCreditBatch(tx);
    vector<uint160> branch = network_state.credit_batch.Branch(credit);
    bool ok = VerifyBranchFromOrderedHashTree(0, credit.getvch(), branch, network_state.GetBatchRoot());
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ANetworkState, AddsThePrecedingMinedCreditToTheCreditBatchWhenSet)
{
    MinedCredit previous_mined_credit;
    Point pubkey(2);
    previous_mined_credit.keydata = pubkey.getvch();
    previous_mined_credit.amount = 1;
    network_state.SetPreviousMinedCredit(previous_mined_credit);
    ASSERT_THAT(network_state.previous_mined_credit, Eq(previous_mined_credit));
    ASSERT_THAT(network_state.credit_batch.size(), Eq(1));
}

TEST_F(ANetworkState, ValidatesIncomingMinedCredits)
{
    MinedCredit incoming_mined_credit;
    Point pubkey(2);
    incoming_mined_credit.keydata = pubkey.getvch();
    incoming_mined_credit.amount = 100000000;
    bool ok = network_state.ValidateIncomingMinedCredit(incoming_mined_credit);
    ASSERT_THAT(ok, Eq(true));
}