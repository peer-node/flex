#include <src/crypto/uint256.h>
#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <src/credits/SignedTransaction.h>
#include <src/credits/CreditBatch.h>
#include "CreditSystem.h"
#include "MinedCreditMessage.h"

using std::set;

uint160 CreditSystem::MostWorkBatch()
{
    LocationIterator work_scanner = creditdata.LocationIterator("total_work");
    work_scanner.SeekEnd();
    uint160 hash(0), total_work(0);
    work_scanner.GetPreviousObjectAndLocation(hash, total_work);
    return hash;
}

void CreditSystem::StoreMinedCredit(MinedCredit mined_credit)
{
    uint160 credit_hash = mined_credit.GetHash160();
    StoreHash(credit_hash);
    creditdata[credit_hash]["mined_credit"] = mined_credit;
    msgdata[credit_hash]["type"] = "mined_credit";
}

void CreditSystem::StoreTransaction(SignedTransaction tx)
{
    uint160 hash = tx.GetHash160();
    StoreHash(hash);
    msgdata[hash]["type"] = "tx";
    creditdata[hash]["tx"] = tx;
}

void CreditSystem::StoreHash(uint160 hash)
{
    uint32_t short_hash = *(uint32_t*)&hash;
    std::vector<uint160> matches = msgdata[short_hash]["matches"];
    if (!VectorContainsEntry(matches, hash))
        matches.push_back(hash);
    msgdata[short_hash]["matches"] = matches;
}

void CreditSystem::StoreMinedCreditMessage(MinedCreditMessage msg)
{
    uint160 hash = msg.GetHash160();
    msgdata[hash]["type"] = "msg";
    msgdata[hash]["msg"] = msg;
    creditdata[msg.mined_credit.GetHash160()]["msg"] = msg;
    creditdata[hash]["msg"] = msg;
    StoreMinedCredit(msg.mined_credit);
}

CreditBatch CreditSystem::ReconstructBatch(MinedCreditMessage &msg)
{
    EncodedNetworkState state = msg.mined_credit.network_state;
    CreditBatch batch(state.previous_mined_credit_hash, state.batch_offset);

    msg.hash_list.RecoverFullHashes(msgdata);

    for (auto message_hash : msg.hash_list.full_hashes)
    {
        std::string type = msgdata[message_hash]["type"];
        if (type == "tx")
        {
            SignedTransaction tx;
            tx = creditdata[message_hash]["tx"];
            batch.AddCredits(tx.rawtx.outputs);
        }
        else if (type == "mined_credit")
        {
            batch.Add(creditdata[message_hash]["mined_credit"]);
        }
    }
    return batch;
}

uint160 CreditSystem::PreviousCreditHash(uint160 credit_hash)
{
    if (credit_hash == 0)
        return 0;
    MinedCredit mined_credit;
    mined_credit = creditdata[credit_hash]["mined_credit"];
    return mined_credit.network_state.previous_mined_credit_hash;
}

uint160 CreditSystem::FindFork(uint160 credit_hash1, uint160 credit_hash2)
{
    set<uint160> seen;

    while (not seen.count(credit_hash1) and not seen.count(credit_hash2))
    {
        if (credit_hash1 == credit_hash2)
            return credit_hash1;

        seen.insert(credit_hash2);
        seen.insert(credit_hash1);

        credit_hash1 = PreviousCreditHash(credit_hash1);
        if (credit_hash1 != 0 and seen.count(credit_hash1))
            return credit_hash1;

        credit_hash2 = PreviousCreditHash(credit_hash2);
        if (credit_hash2 != 0 and seen.count(credit_hash2))
            return credit_hash2;
    }
    return 0;
}

std::set<uint64_t> CreditSystem::GetPositionsOfCreditsSpentInBatch(uint160 credit_hash)
{
    std::set<uint64_t> spent;

    MinedCreditMessage msg;
    msg = creditdata[credit_hash]["msg"];
    msg.hash_list.RecoverFullHashes(msgdata);

    for (auto message_hash : msg.hash_list.full_hashes)
    {
        std::string type = msgdata[message_hash]["type"];
        if (type == "tx")
        {
            SignedTransaction tx;
            tx = creditdata[message_hash]["tx"];
            for (auto credit_in_batch : tx.rawtx.inputs)
                spent.insert(credit_in_batch.position);
        }
    }
    return spent;
}

set<uint64_t> CreditSystem::GetPositionsOfCreditsSpentBetween(uint160 from_credit_hash, uint160 to_credit_hash)
{
    set<uint64_t> spent;

    while (to_credit_hash != from_credit_hash and to_credit_hash != 0)
    {
        auto spent_in_batch = GetPositionsOfCreditsSpentInBatch(to_credit_hash);
        spent.insert(spent_in_batch.begin(), spent_in_batch.end());
        to_credit_hash = PreviousCreditHash(to_credit_hash);
    }
    return spent;
}

bool CreditSystem::GetSpentAndUnspentWhenSwitchingAcrossFork(uint160 from_credit_hash, uint160 to_credit_hash,
                                                             set<uint64_t> &spent, set<uint64_t> &unspent)
{
    uint160 fork = FindFork(from_credit_hash, to_credit_hash);
    if (fork == 0 and from_credit_hash != 0)
        return false;

    spent = GetPositionsOfCreditsSpentBetween(fork, to_credit_hash);
    unspent = GetPositionsOfCreditsSpentBetween(fork, from_credit_hash);
    return true;
}

BitChain CreditSystem::GetSpentChainOnOtherProngOfFork(BitChain &spent_chain, uint160 from_credit_hash,
                                                       uint160 to_credit_hash)
{
    set<uint64_t> spent, unspent;
    if (not GetSpentAndUnspentWhenSwitchingAcrossFork(from_credit_hash, to_credit_hash, spent, unspent))
        return BitChain();
    MinedCredit to_credit = creditdata[to_credit_hash]["mined_credit"];
    uint64_t length = to_credit.network_state.batch_offset + to_credit.network_state.batch_size;
    BitChain resulting_chain = spent_chain;
    resulting_chain.SetLength(length);
    for (auto position : unspent)
        resulting_chain.Clear(position);
    for (auto position : spent)
        resulting_chain.Set(position);
    return resulting_chain;
}

BitChain CreditSystem::GetSpentChain(uint160 credit_hash)
{
    uint160 starting_hash = credit_hash;

    while (starting_hash != 0 and not creditdata[starting_hash].HasProperty("spent_chain"))
        starting_hash = PreviousCreditHash(starting_hash);

    BitChain starting_chain = creditdata[starting_hash]["spent_chain"];
    return GetSpentChainOnOtherProngOfFork(starting_chain, starting_hash, credit_hash);
}




