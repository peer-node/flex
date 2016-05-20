#include <src/credits/SignedTransaction.h>
#include "NetworkState.h"


EncodedNetworkState NetworkState::GetEncodedState()
{
    EncodedNetworkState encoded_state;
}

uint160 NetworkState::GetBatchRoot()
{
    return credit_batch.Root();
}

void NetworkState::AddTransactionOutputsToCreditBatch(SignedTransaction transaction)
{
    for (auto credit : transaction.rawtx.outputs)
    {
        credit_batch.Add(credit);
    }
}

void NetworkState::SetPreviousMinedCredit(MinedCredit credit)
{
    credit_batch.credits.resize(0);
    credit_batch.Add((Credit)credit);
    previous_mined_credit = credit;
}

void NetworkState::AddTransaction(SignedTransaction transaction)
{
    transactions.push_back(transaction);
}

bool NetworkState::ValidateIncomingMinedCredit(MinedCredit incoming_mined_credit)
{
    return false;
}
