#include <src/crypto/uint256.h>
#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <src/credits/SignedTransaction.h>
#include <src/credits/CreditBatch.h>
#include <test/flex_tests/mining/MiningHashTree.h>
#include "CreditSystem.h"
#include "MinedCreditMessage.h"
#include "Calend.h"

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
    uint160 credit_hash = msg.mined_credit.GetHash160();
    msgdata[hash]["type"] = "msg";
    msgdata[hash]["msg"] = msg;
    creditdata[credit_hash]["msg"] = msg;
    creditdata[hash]["msg"] = msg;
    StoreMinedCredit(msg.mined_credit);
    if (IsCalend(credit_hash))
    {
        uint160 diurn_root = msg.mined_credit.network_state.DiurnRoot();
        creditdata[diurn_root]["credit_hash"] = credit_hash;
    }
}

uint160 CreditSystem::PrecedingCalendCreditHash(uint160 credit_hash)
{
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
    uint160 diurn_root = mined_credit.network_state.previous_diurn_root;
    uint160 calend_credit_hash = creditdata[diurn_root]["credit_hash"];
    return calend_credit_hash;
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

bool CreditSystem::IsCalend(uint160 credit_hash)
{
    if (creditdata[credit_hash]["is_calend"])
        return true;
    if (creditdata[credit_hash]["is_not_calend"])
        return false;
    bool is_calend;
    MinedCreditMessage msg = creditdata[credit_hash]["msg"];
    if (msg.mined_credit.network_state.diurnal_difficulty == 0)
        is_calend = false;
    else if (msg.proof_of_work.proof.DifficultyAchieved() >= msg.mined_credit.network_state.diurnal_difficulty)
    {
        creditdata[credit_hash]["is_calend"] = true;
        is_calend = true;
    }
    else
    {
        creditdata[credit_hash]["is_not_calend"] = true;
        is_calend = false;
    }
    return is_calend;
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

uint160 CreditSystem::TotalWork(MinedCreditMessage &msg)
{
    EncodedNetworkState& state = msg.mined_credit.network_state;
    return state.previous_total_work + state.difficulty;
}

void CreditSystem::AddToMainChain(MinedCreditMessage &msg)
{
    uint160 total_work = TotalWork(msg);
    uint160 credit_hash = msg.mined_credit.GetHash160();
    creditdata[credit_hash].Location("main_chain") = total_work;
    creditdata[msg.mined_credit.network_state.batch_root]["credit_hash"] = credit_hash;
}

void CreditSystem::RemoveFromMainChain(MinedCreditMessage &msg)
{
    uint160 credit_hash = msg.mined_credit.GetHash160();
    uint160 recorded_total_work = creditdata[credit_hash].Location("main_chain");
    if (recorded_total_work == TotalWork(msg))
        creditdata.RemoveFromLocation("main_chain", recorded_total_work);
    creditdata[msg.mined_credit.network_state.batch_root]["credit_hash"] = 0;
}

bool CreditSystem::IsInMainChain(uint160 credit_hash)
{
    uint160 recorded_total_work = creditdata[credit_hash].Location("main_chain");
    return recorded_total_work != 0;
}

uint160 AdjustDifficultyAfterBatchInterval(uint160 earlier_difficulty, uint64_t interval)
{
    if (interval > TARGET_BATCH_INTERVAL)
        return (earlier_difficulty * 95) / uint160(100);
    return (earlier_difficulty * 100) / uint160(95);
}

uint160 CreditSystem::GetNextDifficulty(MinedCredit credit)
{
    if (credit.network_state.difficulty == 0)
        return INITIAL_DIFFICULTY;
    uint160 prev_hash = credit.network_state.previous_mined_credit_hash;
    MinedCredit preceding_credit = creditdata[prev_hash]["mined_credit"];
    uint64_t batch_interval = credit.network_state.timestamp - preceding_credit.network_state.timestamp;
    return AdjustDifficultyAfterBatchInterval(credit.network_state.difficulty, batch_interval);
}

uint160 AdjustDiurnalDifficultyAfterDiurnDuration(uint160 earlier_diurnal_difficulty, uint64_t duration)
{
    if (duration > TARGET_DIURN_LENGTH)
        return (earlier_diurnal_difficulty * 95) / uint160(100);
    return (earlier_diurnal_difficulty * 100) / uint160(95);
}

uint160 CreditSystem::GetNextDiurnalDifficulty(MinedCredit credit)
{
    uint160 prev_diurn_root = credit.network_state.previous_diurn_root;
    if (prev_diurn_root == 0)
        return INITIAL_DIURNAL_DIFFICULTY;

    if (not IsCalend(credit.GetHash160()))
        return credit.network_state.diurnal_difficulty;

    uint160 calend_hash = creditdata[prev_diurn_root]["credit_hash"];
    MinedCredit previous_calend_credit = creditdata[calend_hash]["mined_credit"];
    auto prev_state = previous_calend_credit.network_state;
    uint64_t diurn_duration = credit.network_state.timestamp - prev_state.timestamp;
    uint160 earlier_diurnal_difficulty = credit.network_state.diurnal_difficulty;
    return AdjustDiurnalDifficultyAfterDiurnDuration(earlier_diurnal_difficulty, diurn_duration);
}

EncodedNetworkState CreditSystem::SucceedingNetworkState(MinedCredit mined_credit)
{
    EncodedNetworkState next_state, prev_state = mined_credit.network_state;

    next_state.previous_mined_credit_hash = mined_credit.GetHash160();
    next_state.batch_number = prev_state.batch_number + 1;
    next_state.batch_offset = prev_state.batch_offset + prev_state.batch_size;
    next_state.previous_total_work = prev_state.previous_total_work + prev_state.difficulty;
    next_state.difficulty = GetNextDifficulty(mined_credit);
    next_state.diurnal_difficulty = GetNextDiurnalDifficulty(mined_credit);
    if (IsCalend(mined_credit.GetHash160()))
        next_state.previous_diurn_root = SymmetricCombine(prev_state.previous_diurn_root, prev_state.diurnal_block_root);
    else
        next_state.previous_diurn_root = prev_state.previous_diurn_root;

    next_state.network_id = prev_state.network_id;

    // message_list_hash, spent_chain_hash, batch_size, batch_root, timestamp
    // depend on external data

    return next_state;
}

void CreditSystem::SetBatchRootAndSizeAndMessageListHash(MinedCreditMessage& msg)
{
    CreditBatch batch = ReconstructBatch(msg);
    msg.mined_credit.network_state.batch_size = (uint32_t) batch.size();
    msg.mined_credit.network_state.batch_root = batch.Root();
    msg.mined_credit.network_state.message_list_hash = msg.hash_list.GetHash160();
}

void CreditSystem::SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(uint64_t number_of_megabytes)
{
    uint64_t memory_factor = MemoryFactorFromNumberOfMegabytes(number_of_megabytes);
    expected_memory_factor_for_mined_credit_proofs_of_work = memory_factor;
}

bool CreditSystem::CheckHashesSeedsAndThresholdsInMinedCreditMessageProofOfWork(MinedCreditMessage& msg)
{
    bool ok = true;
    auto branch = msg.proof_of_work.branch;
    TwistWorkProof& proof = msg.proof_of_work.proof;

    if (branch.size() == 0 or branch[0] != msg.mined_credit.GetHash())
        ok = false;
    else if (MiningHashTree::EvaluateBranch(branch) != msg.proof_of_work.proof.memoryseed)
        ok = false;
    else if (proof.N_factor != expected_memory_factor_for_mined_credit_proofs_of_work)
        ok = false;
    else if (proof.seeds.size() != FLEX_WORK_NUMBER_OF_SEGMENTS)
        ok = false;
    else if (proof.link_threshold != proof.target * FLEX_WORK_NUMBER_OF_LINKS)
        ok = false;

    return ok;
}

bool CreditSystem::QuickCheckProofOfWorkInMinedCreditMessage(MinedCreditMessage &msg)
{
    uint160 msg_hash = msg.GetHash160();
    if (creditdata[msg_hash]["quickcheck_ok"]) return true;
    if (creditdata[msg_hash]["quickcheck_bad"]) return false;

    bool ok = true;
    auto branch = msg.proof_of_work.branch;
    TwistWorkProof& proof = msg.proof_of_work.proof;

    if (not CheckHashesSeedsAndThresholdsInMinedCreditMessageProofOfWork(msg))
        ok = false;

    if (ok and msg.proof_of_work.proof.DifficultyAchieved() < msg.mined_credit.network_state.difficulty)
        ok = false;

    if (ok) creditdata[msg_hash]["quickcheck_ok"] = true;
    else creditdata[msg_hash]["quickcheck_bad"] = true;

    return ok;
}

bool CreditSystem::QuickCheckProofOfWorkInCalend(Calend calend)
{
    uint160 msg_hash = calend.GetHash160();
    if (creditdata[msg_hash]["quickcalendcheck_ok"]) return true;
    if (creditdata[msg_hash]["quickcalendcheck_bad"]) return false;

    bool ok = true;
    if (not QuickCheckProofOfWorkInMinedCreditMessage(calend))
        ok = false;
    if (calend.proof_of_work.proof.DifficultyAchieved() < calend.mined_credit.network_state.diurnal_difficulty)
        ok = false;

    if (ok) creditdata[msg_hash]["quickcalendcheck_ok"] = true;
    else creditdata[msg_hash]["quickcalendcheck_bad"] = true;

    return ok;
}









