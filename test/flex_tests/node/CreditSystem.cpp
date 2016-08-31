#include <src/crypto/uint256.h>
#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <src/credits/SignedTransaction.h>
#include <src/credits/CreditBatch.h>
#include <test/flex_tests/mining/MiningHashTree.h>
#include <src/base/util_time.h>
#include "CreditSystem.h"
#include "MinedCreditMessage.h"
#include "Calend.h"
#include "CalendarMessage.h"

#include "log.h"
#define LOG_CATEGORY "CreditSystem.cpp"

using std::set;
using std::vector;
using std::string;

vector<uint160> CreditSystem::MostWorkBatches()
{
    LocationIterator work_scanner = creditdata.LocationIterator("total_work");
    work_scanner.SeekEnd();
    vector<uint160> hashes;
    uint160 total_work(0);
    work_scanner.GetPreviousObjectAndLocation(hashes, total_work);
    return hashes;
}

void CreditSystem::StoreMinedCredit(MinedCredit mined_credit)
{
    uint160 credit_hash = mined_credit.GetHash160();
    StoreHash(credit_hash);
    creditdata[credit_hash]["mined_credit"] = mined_credit;
    msgdata[credit_hash]["type"] = "mined_credit";
}

void CreditSystem::RecordTotalWork(uint160 credit_hash, uint160 total_work)
{
    vector<uint160> hashes_at_location;
    creditdata.GetObjectAtLocation(hashes_at_location, "total_work", total_work);
    hashes_at_location.push_back(credit_hash);
    creditdata[hashes_at_location].Location("total_work") = total_work;
}

bool CreditSystem::MinedCreditWasRecordedToHaveTotalWork(uint160 credit_hash, uint160 total_work)
{
    vector<uint160> hashes_at_location;
    creditdata.GetObjectAtLocation(hashes_at_location, "total_work", total_work);
    return VectorContainsEntry(hashes_at_location, credit_hash);
}

void CreditSystem::AcceptMinedCreditAsValidByRecordingTotalWorkAndParent(MinedCredit mined_credit)
{
    uint160 credit_hash = mined_credit.GetHash160();
    uint160 total_work = mined_credit.network_state.difficulty + mined_credit.network_state.previous_total_work;
    RecordTotalWork(credit_hash, total_work);
    SetChildBatch(mined_credit.network_state.previous_mined_credit_hash, credit_hash);
}

void CreditSystem::SetChildBatch(uint160 parent_hash, uint160 child_hash)
{
    vector<uint160> children = creditdata[parent_hash]["children"];
    if (!VectorContainsEntry(children, child_hash))
        children.push_back(child_hash);
    creditdata[parent_hash]["children"] = children;
}

void CreditSystem::StoreTransaction(SignedTransaction tx)
{

    uint160 hash = tx.GetHash160();
    StoreHash(hash);
    msgdata[hash]["type"] = "tx";
    msgdata[hash]["tx"] = tx;
}

void CreditSystem::StoreHash(uint160 hash)
{
    StoreHash(hash, msgdata);
}

void CreditSystem::StoreHash(uint160 hash, MemoryDataStore& hashdata)
{
    uint32_t short_hash = *(uint32_t*)&hash;
    vector<uint160> matches = hashdata[short_hash]["matches"];
    if (!VectorContainsEntry(matches, hash))
        matches.push_back(hash);
    hashdata[short_hash]["matches"] = matches;
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
        creditdata[diurn_root]["calend_credit_hash"] = credit_hash;
    }
}

uint160 CreditSystem::PrecedingCalendCreditHash(uint160 credit_hash)
{
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
    uint160 diurn_root = mined_credit.network_state.previous_diurn_root;
    uint160 calend_credit_hash = creditdata[diurn_root]["calend_credit_hash"];
    return calend_credit_hash;
}

CreditBatch CreditSystem::ReconstructBatch(MinedCreditMessage &msg)
{
    EncodedNetworkState state = msg.mined_credit.network_state;
    CreditBatch batch(state.previous_mined_credit_hash, state.batch_offset);

    msg.hash_list.RecoverFullHashes(msgdata);

    for (auto message_hash : msg.hash_list.full_hashes)
    {
        string type = msgdata[message_hash]["type"];
        if (type == "tx")
        {
            SignedTransaction tx;
            tx = msgdata[message_hash]["tx"];
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

set<uint64_t> CreditSystem::GetPositionsOfCreditsSpentInBatch(uint160 credit_hash)
{
    set<uint64_t> spent;

    MinedCreditMessage msg;
    msg = creditdata[credit_hash]["msg"];
    if (not msg.hash_list.RecoverFullHashes(msgdata))
    {
        log_ << "failed to recover full hashes for " << credit_hash << "\n";
    }

    for (auto message_hash : msg.hash_list.full_hashes)
    {
        string type = msgdata[message_hash]["type"];
        if (type == "tx")
        {
            SignedTransaction tx;
            tx = msgdata[message_hash]["tx"];
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
    spent = GetPositionsOfCreditsSpentBetween(fork, to_credit_hash);
    unspent = GetPositionsOfCreditsSpentBetween(fork, from_credit_hash);
    return true;
}

BitChain CreditSystem::GetSpentChainOnOtherProngOfFork(BitChain &spent_chain, uint160 from_credit_hash,
                                                       uint160 to_credit_hash)
{
    set<uint64_t> spent, unspent;
    if (not GetSpentAndUnspentWhenSwitchingAcrossFork(from_credit_hash, to_credit_hash, spent, unspent))
    {
        log_ << "failed to switch across fork\n";
        return BitChain();
    }
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

void CreditSystem::AddCreditHashAndPredecessorsToMainChain(uint160 credit_hash)
{
    MinedCreditMessage msg = creditdata[credit_hash]["msg"];
    while (not IsInMainChain(credit_hash) and credit_hash != 0)
    {
        AddToMainChain(msg);
        credit_hash = msg.mined_credit.network_state.previous_mined_credit_hash;
        msg = creditdata[credit_hash]["msg"];
    }
}

void CreditSystem::RemoveFromMainChain(MinedCreditMessage &msg)
{
    uint160 credit_hash = msg.mined_credit.GetHash160();
    uint160 recorded_total_work = creditdata[credit_hash].Location("main_chain");
    if (recorded_total_work == TotalWork(msg))
        creditdata.RemoveFromLocation("main_chain", recorded_total_work);
    creditdata[msg.mined_credit.network_state.batch_root]["credit_hash"] = 0;
}

void CreditSystem::RemoveFromMainChain(uint160 credit_hash)
{
    MinedCreditMessage msg = creditdata[credit_hash]["msg"];
    RemoveFromMainChain(msg);
}

void CreditSystem::RemoveFromMainChainAndDeleteRecordOfTotalWork(MinedCreditMessage &msg)
{
    RemoveFromMainChain(msg);
    DeleteRecordOfTotalWork(msg);
}

void CreditSystem::DeleteRecordOfTotalWork(MinedCreditMessage &msg)
{
    uint160 total_work = TotalWork(msg), credit_hash = msg.mined_credit.GetHash160();

    vector<uint160> hashes_with_same_total_work;
    creditdata.GetObjectAtLocation(hashes_with_same_total_work, "total_work", total_work);
    if (VectorContainsEntry(hashes_with_same_total_work, credit_hash))
        EraseEntryFromVector(credit_hash, hashes_with_same_total_work);

    if (hashes_with_same_total_work.size() > 0)
        creditdata[hashes_with_same_total_work].Location("total_work") = total_work;
    else
        creditdata.RemoveFromLocation("total_work", total_work);
}

void CreditSystem::RemoveFromMainChainAndDeleteRecordOfTotalWork(uint160 credit_hash)
{
    MinedCreditMessage msg = creditdata[credit_hash]["msg"];
    RemoveFromMainChainAndDeleteRecordOfTotalWork(msg);
}

void CreditSystem::RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(uint160 credit_hash)
{
    RemoveFromMainChainAndDeleteRecordOfTotalWork(credit_hash);
    vector<uint160> children = creditdata[credit_hash]["children"];
    for (auto child : children)
        RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(child);
}

bool CreditSystem::IsInMainChain(uint160 credit_hash)
{
    uint160 recorded_total_work = creditdata[credit_hash].Location("main_chain");
    return recorded_total_work != 0;
}

std::string CreditSystem::MessageType(uint160 hash)
{
    return msgdata[hash]["type"];
}

void CreditSystem::SwitchMainChainToOtherBranchOfFork(uint160 current_tip, uint160 new_tip)
{
    uint160 fork = FindFork(current_tip, new_tip);
    vector<uint160> to_add_to_main_chain;

    while (current_tip != fork)
    {
        RemoveFromMainChain(current_tip);
        current_tip = PreviousCreditHash(current_tip);
    }
    uint160 hash_on_new_branch = new_tip;
    while (hash_on_new_branch != fork)
    {
        to_add_to_main_chain.push_back(hash_on_new_branch);
        hash_on_new_branch = PreviousCreditHash(hash_on_new_branch);
    }
    std::reverse(to_add_to_main_chain.begin(), to_add_to_main_chain.end());
    for (auto credit_hash : to_add_to_main_chain)
    {
        MinedCreditMessage msg = creditdata[credit_hash]["msg"];
        AddToMainChain(msg);
    }
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
        return initial_difficulty;
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
        return initial_diurnal_difficulty;

    if (not IsCalend(credit.GetHash160()))
    {
        uint160 credit_hash = credit.GetHash160();
        MinedCreditMessage msg = creditdata[credit_hash]["msg"];
        return credit.network_state.diurnal_difficulty;
    }

    uint160 calend_hash = creditdata[prev_diurn_root]["calend_credit_hash"];

    MinedCredit previous_calend_credit = creditdata[calend_hash]["mined_credit"];
    auto prev_state = previous_calend_credit.network_state;
    uint64_t diurn_duration = credit.network_state.timestamp - prev_state.timestamp;
    uint160 earlier_diurnal_difficulty = credit.network_state.diurnal_difficulty;

    return AdjustDiurnalDifficultyAfterDiurnDuration(earlier_diurnal_difficulty, diurn_duration);
}

EncodedNetworkState CreditSystem::SucceedingNetworkState(MinedCredit mined_credit)
{
    EncodedNetworkState next_state, prev_state = mined_credit.network_state;

    next_state.batch_number = prev_state.batch_number + 1;
    if (prev_state.batch_number == 0)
        next_state.previous_mined_credit_hash = 0;
    else
        next_state.previous_mined_credit_hash = mined_credit.GetHash160();
    next_state.batch_offset = prev_state.batch_offset + prev_state.batch_size;
    next_state.previous_total_work = prev_state.previous_total_work + prev_state.difficulty;
    next_state.difficulty = GetNextDifficulty(mined_credit);
    next_state.diurnal_difficulty = GetNextDiurnalDifficulty(mined_credit);
    if (IsCalend(mined_credit.GetHash160()))
        next_state.previous_diurn_root = SymmetricCombine(prev_state.previous_diurn_root, prev_state.diurnal_block_root);
    else
        next_state.previous_diurn_root = prev_state.previous_diurn_root;
    next_state.diurnal_block_root = GetNextDiurnalBlockRoot(mined_credit);

    next_state.network_id = prev_state.network_id;

    next_state.timestamp = GetAdjustedTimeMicros();
    // message_list_hash, spent_chain_hash, batch_size, batch_root
    // depend on external data

    return next_state;
}

void CreditSystem::SetBatchRootAndSizeAndMessageListHashAndSpentChainHash(MinedCreditMessage &msg)
{
    CreditBatch batch = ReconstructBatch(msg);
    msg.mined_credit.network_state.batch_size = (uint32_t) batch.size();
    msg.mined_credit.network_state.batch_root = batch.Root();
    msg.mined_credit.network_state.message_list_hash = msg.hash_list.GetHash160();
    BitChain spent_chain = ConstructSpentChainFromPreviousSpentChainAndContentsOfMinedCreditMessage(msg);
    msg.mined_credit.network_state.spent_chain_hash = spent_chain.GetHash160();
}

BitChain CreditSystem::ConstructSpentChainFromPreviousSpentChainAndContentsOfMinedCreditMessage(MinedCreditMessage& msg)
{
    BitChain spent_chain = GetSpentChain(msg.mined_credit.network_state.previous_mined_credit_hash);
    msg.hash_list.RecoverFullHashes(msgdata);
    for (auto hash : msg.hash_list.full_hashes)
    {
        string type = MessageType(hash);
        if (type == "mined_credit")
            spent_chain.Add();
        else if (type == "tx")
        {
            SignedTransaction tx = msgdata[hash]["tx"];
            for (auto input : tx.rawtx.inputs)
                spent_chain.Set(input.position);
            for (auto output : tx.rawtx.outputs)
                spent_chain.Add();
        }
    }
    return spent_chain;
}

void CreditSystem::SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(uint64_t number_of_megabytes)
{
    uint64_t memory_factor = MemoryFactorFromNumberOfMegabytes(number_of_megabytes);
    expected_memory_factor_for_mined_credit_proofs_of_work = memory_factor;
}

void CreditSystem::SetMiningParameters(uint64_t number_of_megabytes_,
                                       uint160 initial_difficulty_,
                                       uint160 initial_diurnal_difficulty_)
{
    SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(number_of_megabytes_);
    initial_difficulty = initial_difficulty_;
    initial_diurnal_difficulty = initial_diurnal_difficulty_;
}

bool CreditSystem::CheckHashesSeedsAndThresholdsInMinedCreditMessageProofOfWork(MinedCreditMessage& msg)
{
    bool ok = true;
    auto branch = msg.proof_of_work.branch;
    TwistWorkProof& proof = msg.proof_of_work.proof;

    if (branch.size() == 0 or branch[0] != msg.mined_credit.GetHash())
        ok = false;
    if (MiningHashTree::EvaluateBranch(branch) != msg.proof_of_work.proof.memory_seed)
        ok = false;
    if (proof.memory_factor != expected_memory_factor_for_mined_credit_proofs_of_work)
        ok = false;
    if (proof.seeds.size() != FLEX_WORK_NUMBER_OF_SEGMENTS)
        ok = false;
    if (proof.link_threshold != proof.target * FLEX_WORK_NUMBER_OF_LINKS)
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
    {
        ok = false;
    }

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

uint160 CreditSystem::GetNextPreviousDiurnRoot(MinedCredit &mined_credit)
{
    if (creditdata[mined_credit.GetHash160()]["is_calend"])
        return SymmetricCombine(mined_credit.network_state.previous_diurn_root,
                                mined_credit.network_state.diurnal_block_root);
    else
        return mined_credit.network_state.previous_diurn_root;
}

uint160 CreditSystem::GetNextDiurnalBlockRoot(MinedCredit mined_credit)
{
    vector<uint160> credit_hashes;
    uint160 credit_hash = mined_credit.GetHash160();
    if (mined_credit.network_state.batch_number > 0)
        credit_hashes.push_back(credit_hash);
    while (not IsCalend(credit_hash) and credit_hash != 0)
    {
        credit_hash = PreviousCreditHash(credit_hash);
        if (credit_hash != 0)
            credit_hashes.push_back(credit_hash);
    }
    std::reverse(credit_hashes.begin(), credit_hashes.end());
    DiurnalBlock block;
    for (auto hash : credit_hashes)
        block.Add(hash);

    return block.Root();
}

bool CreditSystem::DataIsPresentFromMinedCreditToPrecedingCalendOrStart(MinedCredit mined_credit)
{
    uint160 credit_hash = mined_credit.GetHash160();
    while (credit_hash != 0)
    {
        uint160 previous_credit_hash = PreviousCreditHash(credit_hash);
        if (previous_credit_hash == 0)
        {
            mined_credit = creditdata[credit_hash]["mined_credit"];
            return mined_credit.network_state.batch_number == 1;
        }
        credit_hash = previous_credit_hash;
        if (IsCalend(credit_hash) and creditdata[credit_hash].HasProperty("msg"))
            return true;
    }
    return false;
}

bool CreditSystem::ProofHasCorrectNumberOfSeedsAndLinks(TwistWorkProof proof)
{
    return proof.seeds.size() == FLEX_WORK_NUMBER_OF_SEGMENTS and
            proof.links.size() == proof.link_lengths.size() and
            proof.links.size() >= 10;
}

void CreditSystem::GetMessagesOnOldAndNewBranchesOfFork(uint160 old_tip, uint160 new_tip,
                                                        std::vector<uint160>& messages_on_old_branch,
                                                        std::vector<uint160>& messages_on_new_branch)
{
    uint160 fork = FindFork(old_tip, new_tip);
    messages_on_old_branch = GetMessagesOnBranch(fork, old_tip);
    messages_on_new_branch = GetMessagesOnBranch(fork, new_tip);
}

vector<uint160> CreditSystem::GetMessagesOnBranch(uint160 branch_start, uint160 branch_end)
{
    vector<uint160> message_hashes;
    while (branch_end != branch_start)
    {
        auto enclosed_hashes = GetMessagesInMinedCreditMessage(branch_end);
        message_hashes.insert(message_hashes.end(), enclosed_hashes.begin(), enclosed_hashes.end());
        branch_end = PreviousCreditHash(branch_end);
    }
    return message_hashes;
}

vector<uint160> CreditSystem::GetMessagesInMinedCreditMessage(uint160 msg_hash)
{
    MinedCreditMessage msg = creditdata[msg_hash]["msg"];
    msg.hash_list.RecoverFullHashes(msgdata);
    return msg.hash_list.full_hashes;
}

uint160 CreditSystem::InitialDifficulty()
{
    return initial_difficulty;
}

void CreditSystem::AddIncompleteProofOfWork(MinedCreditMessage& msg)
{
    NetworkSpecificProofOfWork enclosed_proof;
    enclosed_proof.branch.push_back(msg.mined_credit.GetHash());
    uint256 memory_seed = MiningHashTree::EvaluateBranch(enclosed_proof.branch);
    uint64_t memory_factor = expected_memory_factor_for_mined_credit_proofs_of_work;
    TwistWorkProof proof(memory_seed, memory_factor, msg.mined_credit.network_state.difficulty);
    enclosed_proof.proof = proof;
    msg.proof_of_work = enclosed_proof;
}

void CreditSystem::RecordCalendarReportedWork(CalendarMessage calendar_message, uint160 reported_work)
{
    vector<uint160> calendar_message_hashes;
    creditdata.GetObjectAtLocation(calendar_message_hashes, "reported_calendar_work", reported_work);
    calendar_message_hashes.push_back(calendar_message.GetHash160());
    creditdata[calendar_message_hashes].Location("reported_calendar_work") = reported_work;
}

void CreditSystem::RecordCalendarScrutinizedWork(CalendarMessage calendar_message, uint160 scrutinized_work)
{
    vector<uint160> calendar_message_hashes;
    creditdata.GetObjectAtLocation(calendar_message_hashes, "scrutinized_calendar_work", scrutinized_work);
    calendar_message_hashes.push_back(calendar_message.GetHash160());
    creditdata[calendar_message_hashes].Location("scrutinized_calendar_work") = scrutinized_work;
}

uint160 CreditSystem::MaximumReportedCalendarWork()
{
    vector<uint160> calendar_message_hashes;
    uint160 reported_work = 0;
    LocationIterator work_scanner = creditdata.LocationIterator("reported_calendar_work");
    work_scanner.SeekEnd();
    work_scanner.GetPreviousObjectAndLocation(calendar_message_hashes, reported_work);
    return reported_work;
}

CalendarMessage CreditSystem::CalendarMessageWithMaximumReportedWork()
{
    vector<uint160> calendar_message_hashes;
    uint160 reported_work = 0;
    LocationIterator work_scanner = creditdata.LocationIterator("reported_calendar_work");
    work_scanner.SeekEnd();
    work_scanner.GetPreviousObjectAndLocation(calendar_message_hashes, reported_work);

    if (calendar_message_hashes.size() == 0)
        return CalendarMessage();

    CalendarMessage calendar_message = msgdata[calendar_message_hashes[0]]["calendar"];
    return calendar_message;
}

CalendarMessage CreditSystem::CalendarMessageWithMaximumScrutinizedWork()
{
    vector<uint160> calendar_message_hashes;
    uint160 reported_work = 0;
    LocationIterator work_scanner = creditdata.LocationIterator("scrutinized_calendar_work");
    work_scanner.SeekEnd();
    work_scanner.GetPreviousObjectAndLocation(calendar_message_hashes, reported_work);

    if (calendar_message_hashes.size() == 0)
    {
        return CalendarMessage();
    }

    CalendarMessage calendar_message = msgdata[calendar_message_hashes[0]]["calendar"];
    return calendar_message;
}