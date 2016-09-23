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

void AddToHashesAtLocation(uint160 hash, uint160 location, std::string location_name, MemoryDataStore& datastore)
{
    vector<uint160> hashes_at_location;
    datastore.GetObjectAtLocation(hashes_at_location, location_name, location);
    if (not VectorContainsEntry(hashes_at_location, hash))
        hashes_at_location.push_back(hash);
    datastore[hashes_at_location].Location(location_name) = location;
}

void AddToHashes(uint160 object_hash, std::string property_name, uint160 hash_to_add, MemoryDataStore& datastore)
{
    vector<uint160> hashes = datastore[object_hash][property_name];
    if (not VectorContainsEntry(hashes, hash_to_add))
        hashes.push_back(hash_to_add);
    datastore[object_hash][property_name] = hashes;
}

void RemoveFromHashes(uint160 object_hash, std::string property_name, uint160 hash_to_remove, MemoryDataStore& datastore)
{
    vector<uint160> hashes = datastore[object_hash][property_name];
    if (VectorContainsEntry(hashes, hash_to_remove))
        EraseEntryFromVector(hash_to_remove, hashes);
    datastore[object_hash][property_name] = hashes;
}

void RemoveFromHashesAtLocation(uint160 hash, uint160 location, std::string location_name, MemoryDataStore& datastore)
{
    vector<uint160> hashes_at_location;
    datastore.GetObjectAtLocation(hashes_at_location, location_name, location);
    if (VectorContainsEntry(hashes_at_location, hash))
        EraseEntryFromVector(hash, hashes_at_location);
    if (hashes_at_location.size() > 0)
        datastore[hashes_at_location].Location(location_name) = location;
    else
        datastore.RemoveFromLocation(location_name, location);
}

void CreditSystem::RecordTotalWork(uint160 credit_hash, uint160 total_work)
{
    AddToHashesAtLocation(credit_hash, total_work, "total_work", creditdata);
}

bool CreditSystem::MinedCreditWasRecordedToHaveTotalWork(uint160 credit_hash, uint160 total_work)
{
    vector<uint160> hashes_at_location;
    creditdata.GetObjectAtLocation(hashes_at_location, "total_work", total_work);
    return VectorContainsEntry(hashes_at_location, credit_hash);
}

void CreditSystem::AcceptMinedCreditMessageAsValidByRecordingTotalWorkAndParent(MinedCreditMessage msg)
{
    uint160 msg_hash = msg.GetHash160();
    uint160 total_work = msg.mined_credit.network_state.difficulty + msg.mined_credit.network_state.previous_total_work;
    RecordTotalWork(msg_hash, total_work);
    SetChildBatch(msg.mined_credit.network_state.previous_mined_credit_message_hash, msg_hash);
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
    uint160 msg_hash = msg.GetHash160();
    StoreHash(msg_hash);
    msgdata[msg_hash]["type"] = "msg";
    msgdata[msg_hash]["msg"] = msg;
    creditdata[msg_hash]["msg"] = msg;
    if (IsCalend(msg_hash))
    {
        uint160 diurn_root = msg.mined_credit.network_state.DiurnRoot();
        creditdata[diurn_root]["calend_hash"] = msg_hash;
    }
}

uint160 CreditSystem::PrecedingCalendHash(uint160 msg_hash)
{
    MinedCreditMessage msg = msgdata[msg_hash]["msg"];
    uint160 diurn_root = msg.mined_credit.network_state.previous_diurn_root;
    if (diurn_root == 0)
        return 0;
    uint160 calend_hash = creditdata[diurn_root]["calend_hash"];
    return creditdata[diurn_root]["calend_hash"];
}

CreditBatch CreditSystem::ReconstructBatch(MinedCreditMessage &msg)
{
    EncodedNetworkState state = msg.mined_credit.network_state;
    CreditBatch batch(state.previous_mined_credit_message_hash, state.batch_offset);

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
        else if (type == "msg")
        {
            MinedCreditMessage msg_ = msgdata[message_hash]["msg"];
            batch.Add(msg_.mined_credit);
        }
    }
    return batch;
}

bool CreditSystem::IsCalend(uint160 msg_hash)
{
    if (creditdata[msg_hash]["is_calend"])
        return true;
    if (creditdata[msg_hash]["is_not_calend"])
        return false;

    bool is_calend;
    MinedCreditMessage msg = msgdata[msg_hash]["msg"];
    if (msg.mined_credit.network_state.diurnal_difficulty == 0)
        is_calend = false;
    else if (msg.proof_of_work.proof.DifficultyAchieved() >= msg.mined_credit.network_state.diurnal_difficulty)
    {
        creditdata[msg_hash]["is_calend"] = true;
        is_calend = true;
    }
    else
    {
        creditdata[msg_hash]["is_not_calend"] = true;
        is_calend = false;
    }
    return is_calend;
}

uint160 CreditSystem::PreviousMinedCreditMessageHash(uint160 msg_hash)
{
    if (msg_hash == 0)
        return 0;
    MinedCreditMessage msg = msgdata[msg_hash]["msg"];
    return msg.mined_credit.network_state.previous_mined_credit_message_hash;
}

uint160 CreditSystem::FindFork(uint160 msg_hash1, uint160 msg_hash2)
{
    set<uint160> seen;
    while (not seen.count(msg_hash1) and not seen.count(msg_hash2))
    {
        if (msg_hash1 == msg_hash2)
            return msg_hash1;

        seen.insert(msg_hash2);
        seen.insert(msg_hash1);

        msg_hash1 = PreviousMinedCreditMessageHash(msg_hash1);
        if (msg_hash1 != 0 and seen.count(msg_hash1))
            return msg_hash1;

        msg_hash2 = PreviousMinedCreditMessageHash(msg_hash2);
        if (msg_hash2 != 0 and seen.count(msg_hash2))
            return msg_hash2;
    }
    return 0;
}

set<uint64_t> CreditSystem::GetPositionsOfCreditsSpentInBatch(uint160 msg_hash)
{
    set<uint64_t> spent;

    MinedCreditMessage msg = msgdata[msg_hash]["msg"];
    if (not msg.hash_list.RecoverFullHashes(msgdata))
    {
        // log_ << "failed to recover full hashes for " << msg_hash << "\n";
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

set<uint64_t> CreditSystem::GetPositionsOfCreditsSpentBetween(uint160 from_msg_hash, uint160 to_msg_hash)
{
    set<uint64_t> spent;

    while (to_msg_hash != from_msg_hash and to_msg_hash != 0)
    {
        auto spent_in_batch = GetPositionsOfCreditsSpentInBatch(to_msg_hash);
        spent.insert(spent_in_batch.begin(), spent_in_batch.end());
        to_msg_hash = PreviousMinedCreditMessageHash(to_msg_hash);
    }
    return spent;
}

bool CreditSystem::GetSpentAndUnspentWhenSwitchingAcrossFork(uint160 from_msg_hash, uint160 to_msg_hash,
                                                             set<uint64_t> &spent, set<uint64_t> &unspent)
{
    uint160 fork = FindFork(from_msg_hash, to_msg_hash);
    spent = GetPositionsOfCreditsSpentBetween(fork, to_msg_hash);
    unspent = GetPositionsOfCreditsSpentBetween(fork, from_msg_hash);
    return true;
}

BitChain CreditSystem::GetSpentChainOnOtherProngOfFork(BitChain &spent_chain, uint160 from_msg_hash,
                                                       uint160 to_msg_hash)
{
    set<uint64_t> spent, unspent;
    if (not GetSpentAndUnspentWhenSwitchingAcrossFork(from_msg_hash, to_msg_hash, spent, unspent))
    {
        log_ << "failed to switch across fork\n";
        return BitChain();
    }
    MinedCreditMessage to_msg = msgdata[to_msg_hash]["msg"];
    uint64_t length = to_msg.mined_credit.network_state.batch_offset + to_msg.mined_credit.network_state.batch_size;
    BitChain resulting_chain = spent_chain;
    resulting_chain.SetLength(length);
    for (auto position : unspent)
        resulting_chain.Clear(position);
    for (auto position : spent)
        resulting_chain.Set(position);
    return resulting_chain;
}

BitChain CreditSystem::GetSpentChain(uint160 msg_hash)
{
    uint160 starting_hash = msg_hash;

    while (starting_hash != 0 and not creditdata[starting_hash].HasProperty("spent_chain"))
        starting_hash = PreviousMinedCreditMessageHash(starting_hash);

    BitChain starting_chain = creditdata[starting_hash]["spent_chain"];
    return GetSpentChainOnOtherProngOfFork(starting_chain, starting_hash, msg_hash);
}

uint160 CreditSystem::TotalWork(MinedCreditMessage &msg)
{
    EncodedNetworkState& state = msg.mined_credit.network_state;
    return state.previous_total_work + state.difficulty;
}

void CreditSystem::AddToMainChain(MinedCreditMessage &msg)
{
    uint160 msg_hash = msg.GetHash160();
    creditdata[msg_hash].Location("main_chain") = msg.mined_credit.ReportedWork();
    creditdata[msg.mined_credit.network_state.batch_root]["msg_hash"] = msg_hash;
}

void CreditSystem::AddMinedCreditMessageAndPredecessorsToMainChain(uint160 msg_hash)
{
    MinedCreditMessage msg = msgdata[msg_hash]["msg"];
    while (not IsInMainChain(msg_hash) and msg_hash != 0)
    {
        AddToMainChain(msg);
        msg_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
        msg = msgdata[msg_hash]["msg"];
    }
}

void CreditSystem::RemoveFromMainChain(MinedCreditMessage &msg)
{
    uint160 msg_hash = msg.GetHash160();
    uint160 recorded_total_work = creditdata[msg_hash].Location("main_chain");
    if (recorded_total_work == TotalWork(msg))
        creditdata.RemoveFromLocation("main_chain", recorded_total_work);
    creditdata[msg.mined_credit.network_state.batch_root]["msg_hash"] = 0;
}

void CreditSystem::RemoveFromMainChain(uint160 msg_hash)
{
    MinedCreditMessage msg = msgdata[msg_hash]["msg"];
    RemoveFromMainChain(msg);
}

void CreditSystem::RemoveFromMainChainAndDeleteRecordOfTotalWork(MinedCreditMessage &msg)
{
    RemoveFromMainChain(msg);
    DeleteRecordOfTotalWork(msg);
}

void CreditSystem::DeleteRecordOfTotalWork(MinedCreditMessage &msg)
{
    uint160 total_work = TotalWork(msg), msg_hash = msg.GetHash160();

    vector<uint160> hashes_with_same_total_work;
    creditdata.GetObjectAtLocation(hashes_with_same_total_work, "total_work", total_work);
    if (VectorContainsEntry(hashes_with_same_total_work, msg_hash))
        EraseEntryFromVector(msg_hash, hashes_with_same_total_work);
    RemoveFromHashesAtLocation(msg_hash, total_work, "total_work", creditdata);

    if (hashes_with_same_total_work.size() > 0)
        creditdata[hashes_with_same_total_work].Location("total_work") = total_work;
    else
        creditdata.RemoveFromLocation("total_work", total_work);
}

void CreditSystem::RemoveFromMainChainAndDeleteRecordOfTotalWork(uint160 msg_hash)
{
    MinedCreditMessage msg = msgdata[msg_hash]["msg"];
    RemoveFromMainChainAndDeleteRecordOfTotalWork(msg);
}

void CreditSystem::RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(uint160 credit_hash)
{
    RemoveFromMainChainAndDeleteRecordOfTotalWork(credit_hash);
    vector<uint160> children = creditdata[credit_hash]["children"];
    for (auto child : children)
        RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(child);
}

bool CreditSystem::IsInMainChain(uint160 msg_hash)
{
    uint160 recorded_total_work = creditdata[msg_hash].Location("main_chain");
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
        current_tip = PreviousMinedCreditMessageHash(current_tip);
    }
    uint160 hash_on_new_branch = new_tip;
    while (hash_on_new_branch != fork)
    {
        to_add_to_main_chain.push_back(hash_on_new_branch);
        hash_on_new_branch = PreviousMinedCreditMessageHash(hash_on_new_branch);
    }
    std::reverse(to_add_to_main_chain.begin(), to_add_to_main_chain.end());
    for (auto msg_hash : to_add_to_main_chain)
    {
        MinedCreditMessage msg = msgdata[msg_hash]["msg"];
        AddToMainChain(msg);
    }
}

uint160 AdjustDifficultyAfterBatchInterval(uint160 earlier_difficulty, uint64_t interval)
{
    if (interval > TARGET_BATCH_INTERVAL)
        return (earlier_difficulty * 95) / uint160(100);
    return (earlier_difficulty * 100) / uint160(95);
}

uint160 CreditSystem::GetNextDifficulty(MinedCreditMessage msg)
{
    if (msg.mined_credit.network_state.difficulty == 0)
        return initial_difficulty;
    uint160 prev_hash = msg.mined_credit.network_state.previous_mined_credit_message_hash;
    MinedCreditMessage preceding_msg = msgdata[prev_hash]["msg"];
    uint64_t batch_interval = msg.mined_credit.network_state.timestamp - preceding_msg.mined_credit.network_state.timestamp;
    return AdjustDifficultyAfterBatchInterval(msg.mined_credit.network_state.difficulty, batch_interval);
}

uint160 AdjustDiurnalDifficultyAfterDiurnDuration(uint160 earlier_diurnal_difficulty, uint64_t duration)
{
    if (duration > TARGET_DIURN_LENGTH)
        return (earlier_diurnal_difficulty * 95) / uint160(100);
    return (earlier_diurnal_difficulty * 100) / uint160(95);
}

uint160 CreditSystem::GetNextDiurnalDifficulty(MinedCreditMessage msg)
{
    uint160 prev_diurn_root = msg.mined_credit.network_state.previous_diurn_root;
    if (prev_diurn_root == 0)
        return initial_diurnal_difficulty;

    if (not IsCalend(msg.GetHash160()))
    {
        uint160 msg_hash = msg.GetHash160();
        MinedCreditMessage msg_ = msgdata[msg_hash]["msg"];
        return msg_.mined_credit.network_state.diurnal_difficulty;
    }

    uint160 calend_hash = creditdata[prev_diurn_root]["calend_hash"];

    MinedCreditMessage previous_calend = msgdata[calend_hash]["msg"];
    auto prev_state = previous_calend.mined_credit.network_state;
    uint64_t diurn_duration = msg.mined_credit.network_state.timestamp - prev_state.timestamp;
    uint160 earlier_diurnal_difficulty = msg.mined_credit.network_state.diurnal_difficulty;

    return AdjustDiurnalDifficultyAfterDiurnDuration(earlier_diurnal_difficulty, diurn_duration);
}

EncodedNetworkState CreditSystem::SucceedingNetworkState(MinedCreditMessage msg)
{
    EncodedNetworkState next_state, prev_state = msg.mined_credit.network_state;

    next_state.batch_number = prev_state.batch_number + 1;
    if (prev_state.batch_number == 0)
        next_state.previous_mined_credit_message_hash = 0;
    else
        next_state.previous_mined_credit_message_hash = msg.GetHash160();

    next_state.batch_offset = prev_state.batch_offset + prev_state.batch_size;
    next_state.previous_total_work = prev_state.previous_total_work + prev_state.difficulty;
    next_state.difficulty = GetNextDifficulty(msg);
    next_state.diurnal_difficulty = GetNextDiurnalDifficulty(msg);
    if (IsCalend(msg.GetHash160()))
            next_state.previous_diurn_root = SymmetricCombine(prev_state.previous_diurn_root,
                                                              prev_state.diurnal_block_root);
    else
        next_state.previous_diurn_root = prev_state.previous_diurn_root;
    next_state.diurnal_block_root = GetNextDiurnalBlockRoot(msg);

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
    BitChain spent_chain = GetSpentChain(msg.mined_credit.network_state.previous_mined_credit_message_hash);
    msg.hash_list.RecoverFullHashes(msgdata);
    for (auto hash : msg.hash_list.full_hashes)
    {
        string type = MessageType(hash);
        if (type == "msg")
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

uint160 CreditSystem::GetNextPreviousDiurnRoot(MinedCreditMessage &msg)
{
    if (creditdata[msg.GetHash160()]["is_calend"])
        return SymmetricCombine(msg.mined_credit.network_state.previous_diurn_root,
                                msg.mined_credit.network_state.diurnal_block_root);
    else
        return msg.mined_credit.network_state.previous_diurn_root;
}

uint160 CreditSystem::GetNextDiurnalBlockRoot(MinedCreditMessage msg)
{
    vector<uint160> credit_hashes;
    uint160 credit_hash = msg.mined_credit.GetHash160();
    uint160 msg_hash = msg.GetHash160();
    if (msg.mined_credit.network_state.batch_number > 0)
        credit_hashes.push_back(credit_hash);
    while (not IsCalend(msg_hash) and msg_hash != 0)
    {
        MinedCreditMessage msg_ = msgdata[msg_hash]["msg"];
        msg_hash = msg_.mined_credit.network_state.previous_mined_credit_message_hash;

        if (msg_hash != 0)
        {
            credit_hash = msg_.mined_credit.GetHash160();
            credit_hashes.push_back(credit_hash);
        }
    }
    std::reverse(credit_hashes.begin(), credit_hashes.end());
    DiurnalBlock block;
    for (auto hash : credit_hashes)
        block.Add(hash);

    return block.Root();
}

bool CreditSystem::DataIsPresentFromMinedCreditToPrecedingCalendOrStart(MinedCreditMessage msg)
{
    uint160 msg_hash = msg.GetHash160();
    while (msg_hash != 0)
    {
        uint160 previous_msg_hash = PreviousMinedCreditMessageHash(msg_hash);
        if (previous_msg_hash == 0)
        {
            msg = msgdata[msg_hash]["msg"];
            return msg.mined_credit.network_state.batch_number == 1;
        }
        msg_hash = previous_msg_hash;
        if (IsCalend(msg_hash) and msgdata[msg_hash].HasProperty("msg"))
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
        branch_end = PreviousMinedCreditMessageHash(branch_end);
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

void CreditSystem::MarkCalendAndSucceedingCalendsAsInvalid(CalendarFailureMessage message)
{
    uint160 calend_hash = message.details.mined_credit_message_hash;
    uint160 failure_message_hash = message.GetHash160();
    MarkCalendAndSucceedingCalendsAsInvalid(calend_hash, failure_message_hash);
}

void CreditSystem::MarkCalendAndSucceedingCalendsAsInvalid(uint160 calend_hash, uint160 failure_message_hash)
{
    creditdata[calend_hash]["invalid"] = true;
    creditdata[calend_hash]["failure_message_hash"] = failure_message_hash;

    vector<uint160> succeeding_calends = creditdata[calend_hash]["succeeding_calends"];
    for (auto succeeding_calend_hash : succeeding_calends)
        MarkCalendAndSucceedingCalendsAsInvalid(succeeding_calend_hash, failure_message_hash);
}

bool CreditSystem::CalendHasReportedFailure(uint160 calend_hash)
{
    vector<uint160> reported_failure_message_hashes = creditdata[calend_hash]["reported_failures"];
    return reported_failure_message_hashes.size() > 0;
}

void CreditSystem::RecordReportedFailureOfCalend(CalendarFailureMessage message)
{
    uint160 failure_message_hash = message.GetHash160();
    uint160 calend_hash = message.details.mined_credit_message_hash;

    AddToHashes(calend_hash, "reported_failures", failure_message_hash, creditdata);
}

bool CreditSystem::ReportedFailedCalendHasBeenReceived(CalendarFailureMessage message)
{
    uint160 calend_hash = message.details.mined_credit_message_hash;
    MinedCreditMessage msg = msgdata[calend_hash]["msg"];
    return IsCalend(msg.GetHash160());
}

bool CreditSystem::CalendarContainsAKnownBadCalend(Calendar &calendar_)
{
    for (auto calend : calendar_.calends)
    {
        if (creditdata[calend.GetHash160()].HasProperty("failure_details"))
            return true;
    }
    return false;
}

void CreditSystem::StoreCalendsFromCalendar(Calendar &calendar_)
{
    for (auto calend : calendar_.calends)
    {
        msgdata[calend.GetHash160()]["msg"] = calend;
    }
}

void CreditSystem::RemoveReportedTotalWorkOfMinedCreditsSucceedingInvalidCalend(CalendarFailureMessage message)
{
    uint160 calend_hash = message.details.mined_credit_message_hash;
    MinedCreditMessage calend = msgdata[calend_hash]["msg"];
    LocationIterator work_scanner = creditdata.LocationIterator("total_work");
    work_scanner.Seek(calend.mined_credit.ReportedWork());

    RemoveFromHashesAtLocation(calend_hash, calend.mined_credit.ReportedWork(), "total_work", creditdata);

    std::vector<uint160> msg_hashes;
    uint160 reported_work;

    uint160 previous_diurn_root = calend.mined_credit.network_state.DiurnRoot();
    uint160 previous_msg_hash = calend.GetHash160();

    work_scanner = creditdata.LocationIterator("total_work");
    work_scanner.Seek(calend.mined_credit.ReportedWork());
    while (work_scanner.GetNextObjectAndLocation(msg_hashes, reported_work))
    {
        for (auto msg_hash : msg_hashes)
        {
            MinedCreditMessage msg = msgdata[msg_hash]["msg"];
            if (msg.mined_credit.network_state.previous_mined_credit_message_hash == previous_msg_hash or
                    msg.mined_credit.network_state.previous_diurn_root == previous_diurn_root)
            {
                RemoveFromHashesAtLocation(msg_hash, reported_work, "total_work", creditdata);
                previous_msg_hash = msg_hash;
                previous_diurn_root = msg.mined_credit.network_state.previous_diurn_root;
                work_scanner = creditdata.LocationIterator("total_work");
                work_scanner.Seek(reported_work); // need new iterator because total_work dimension has changed
            }
        }
    }
}
