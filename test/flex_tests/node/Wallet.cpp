#include <src/credits/CreditInBatch.h>
#include <src/crypto/point.h>
#include <src/crypto/secp256k1point.h>
#include <src/vector_tools.h>
#include <src/credits/SignedTransaction.h>
#include <src/credits/creditsign.h>
#include "Wallet.h"
#include "MinedCreditMessage.h"
#include "CreditSystem.h"

std::vector<CreditInBatch> Wallet::GetCredits()
{
    return credits;
}

Point Wallet::GetNewPublicKey()
{
    CBigNum private_key;
    private_key.Randomize(Secp256k1Point::Modulus());
    Point public_key(SECP256K1, private_key);
    keydata[public_key]["privkey"] = private_key;
    return public_key;
}

void Wallet::HandleCreditInBatch(CreditInBatch credit_in_batch)
{
    if (PrivateKeyIsKnown(credit_in_batch) and not HaveCreditInBatchAlready(credit_in_batch))
    {
        credits.push_back(credit_in_batch);
    }
}

bool Wallet::PrivateKeyIsKnown(CreditInBatch credit_in_batch)
{
    Point pubkey;
    pubkey.setvch(credit_in_batch.keydata);
    return keydata[pubkey].HasProperty("privkey");
}

bool Wallet::HaveCreditInBatchAlready(CreditInBatch credit_in_batch)
{
    return VectorContainsEntry(credits, credit_in_batch);
}

SignedTransaction Wallet::GetSignedTransaction(Point pubkey, uint64_t amount)
{
    return SignTransaction(GetUnsignedTransaction(pubkey.getvch(), amount), keydata);
}

SignedTransaction Wallet::GetSignedTransaction(uint160 keyhash, uint64_t amount)
{
    vch_t key_data(keyhash.begin(), keyhash.end());
    return SignTransaction(GetUnsignedTransaction(key_data, amount), keydata);
}

UnsignedTransaction Wallet::GetUnsignedTransaction(vch_t key_data, uint64_t amount)
{
    UnsignedTransaction raw_tx;
    uint64_t amount_in = 0;
    for (auto credit : credits)
    {
        raw_tx.inputs.push_back(credit);
        amount_in += credit.amount;
        if (amount_in >= amount)
            break;
    }

    if (amount < amount_in)
    {
        raw_tx.inputs.resize(0);
        return raw_tx;
    }

    raw_tx.AddOutput(Credit(key_data, amount));

    int64_t change = amount_in - amount;
    if (change > 0)
    {
        Point change_pubkey = GetNewPublicKey();
        raw_tx.outputs.push_back(Credit(change_pubkey.getvch(), (uint64_t) change));
    }

    return raw_tx;
}

void Wallet::AddBatchToTip(MinedCreditMessage msg, CreditSystem* credit_system)
{
    msg.hash_list.RecoverFullHashes(credit_system->msgdata);
    RemoveTransactionInputsSpentInBatchFromCredits(msg, credit_system);
    AddNewCreditsFromBatch(msg, credit_system);
    std::sort(credits.begin(), credits.end());
}

void Wallet::RemoveTransactionInputsSpentInBatchFromCredits(MinedCreditMessage& msg, CreditSystem* credit_system)
{
    for (auto hash : msg.hash_list.full_hashes)
        if (credit_system->MessageType(hash) == "tx")
        {
            SignedTransaction tx = credit_system->creditdata[hash]["tx"];
            for (auto input : tx.rawtx.inputs)
                if (VectorContainsEntry(credits, input))
                    EraseEntryFromVector(input, credits);
        }
}

void Wallet::AddNewCreditsFromBatch(MinedCreditMessage& msg, CreditSystem* credit_system)
{
    CreditBatch batch = credit_system->ReconstructBatch(msg);
    for (auto credit : batch.credits)
    {
        auto credit_in_batch = batch.GetCreditInBatch(credit);
        if (PrivateKeyIsKnown(credit_in_batch) and not VectorContainsEntry(credits, credit_in_batch))
            credits.push_back(credit_in_batch);
    }
}

uint64_t Wallet::Balance()
{
    uint64_t balance = 0;
    for (auto credit : credits)
        balance += credit.amount;
    return balance;
}

void Wallet::RemoveBatchFromTip(MinedCreditMessage msg, CreditSystem *credit_system)
{
    msg.hash_list.RecoverFullHashes(credit_system->msgdata);
    AddTransactionInputsSpentInRemovedBatchToCredits(msg, credit_system);
    RemoveCreditsFromRemovedBatch(msg, credit_system);
    std::sort(credits.begin(), credits.end());
}

void Wallet::AddTransactionInputsSpentInRemovedBatchToCredits(MinedCreditMessage& msg, CreditSystem* credit_system)
{
    for (auto hash : msg.hash_list.full_hashes)
        if (credit_system->MessageType(hash) == "tx")
        {
            SignedTransaction tx = credit_system->creditdata[hash]["tx"];
            for (auto input : tx.rawtx.inputs)
                if (PrivateKeyIsKnown(input) and not VectorContainsEntry(credits, input))
                    credits.push_back(input);
        }
}

void Wallet::RemoveCreditsFromRemovedBatch(MinedCreditMessage& msg, CreditSystem* credit_system)
{
    CreditBatch batch = credit_system->ReconstructBatch(msg);
    for (auto credit : batch.credits)
    {
        auto credit_in_batch = batch.GetCreditInBatch(credit);
        if (VectorContainsEntry(credits, credit_in_batch))
            EraseEntryFromVector(credit_in_batch, credits);
    }
}