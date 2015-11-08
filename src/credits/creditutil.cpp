#include "credits/creditutil.h"
#include "flexnode/flexnode.h"
#include <sstream>

#include "log.h"
#define LOG_CATEGORY "creditutil.cpp"

using namespace std;


bool CheckBranches(CreditInBatch& credit)
{
    Credit raw_credit(credit.keydata, credit.amount);
    uint160 batch_root = credit.branch.back();
    vch_t raw_credit_data = raw_credit.getvch();
    
    if (!VerifyBranch(credit.position, raw_credit_data, 
                      credit.branch, batch_root))
    {
        log_ << "credit branch failed\n";
        return false;
    }
    if (credit.diurn_branch.size() != 1)
    {
        uint160 diurn_root;
        diurn_root = EvaluateBranchWithHash(credit.diurn_branch, batch_root);
        if (diurn_root != credit.diurn_branch.back())
        {
            log_ << "diurn branch failed: " << diurn_root << " vs "
                 << credit.diurn_branch.back() << "\n";
            return false;
        }
    }
    return true;
}

bool CreditInBatchHasValidConnectionToCalendar(CreditInBatch& credit)
{
    log_ << "CreditInBatchHasValidConnectionToCalendar()\n";
    if (credit.diurn_branch.size() == 0 || credit.branch.size() == 0)
    {
        log_ << "missing branch!" << credit.diurn_branch.size()
             << " vs " << credit.branch.size() << "\n";
        return false;
    }

    if (credit.diurn_branch.size() != 1)
    {
        uint160 diurn_root = credit.diurn_branch.back();

        if (!flexnode.calendar.ContainsDiurn(diurn_root))
        {
            log_ << "diurn root " << diurn_root << " is not in calendar\n";
            return false;
        }
    }
    else
    {
        uint160 mined_credit_hash = SymmetricCombine(credit.diurn_branch[0],
                                                     credit.branch.back());
        if (!InMainChain(mined_credit_hash))
        {
            log_ << mined_credit_hash << " is not in main chain\n";
            return false;
        }
    }
    return CheckBranches(credit);
}

std::vector<uint160> GetDiurnBranch(uint160 credit_hash)
{
    std::vector<uint160> diurn_branch;

    if (creditdata[credit_hash].HasProperty("branch"))
    {
        diurn_branch = creditdata[credit_hash]["branch"];
        return diurn_branch;
    }
    if (!InMainChain(credit_hash))
        return diurn_branch;

    uint160 calend_credit_hash = GetNextCalendCreditHash(credit_hash);
    if (calend_credit_hash == 0)
        return diurn_branch;

    MinedCredit calend_credit = creditdata[calend_credit_hash]["mined_credit"];
    uint160 diurn_root = SymmetricCombine(calend_credit.previous_diurn_root,
                                          calend_credit.diurnal_block_root);
    log_ << "GetDiurnBranch(): diurn_root = " << diurn_root << "\n";
    log_ << "exists in calendardata: "
         << calendardata[diurn_root].HasProperty("diurn") << "\n";

    Diurn diurn = calendardata[diurn_root]["diurn"];
    return diurn.Branch(credit_hash);
}

uint160 GetCalendCreditHash(uint160 credit_hash)
{
    MinedCreditMessage msg = creditdata[credit_hash]["msg"];

    uint160 prev_diurn_root = msg.mined_credit.previous_diurn_root;
    if (calendardata[prev_diurn_root].HasProperty("credit_hash"))
        return calendardata[prev_diurn_root]["credit_hash"];

    uint160 calend_hash = PreviousCreditHash(credit_hash);

    MinedCredit mined_credit = creditdata[calend_hash]["mined_credit"];
    while (mined_credit.batch_number && 
           !calendardata[calend_hash]["is_calend"])
    {
        calend_hash = mined_credit.previous_mined_credit_hash;
        mined_credit = creditdata[calend_hash]["mined_credit"];
    }

    return calend_hash;
}

vector<uint160> GetCreditHashesSinceCalend(uint160 calend_credit_hash,
                                           uint160 final_hash)
{
    log_ << "GetCreditHashesSinceCalend(): from " << calend_credit_hash
         << " to " << final_hash << "\n";
    MinedCredit mined_credit = creditdata[final_hash]["mined_credit"];

    log_ << "Final credit is " << final_hash << "\n";
    
    vector<uint160> credit_hashes;

    if (final_hash == calend_credit_hash)
    {
        log_ << "final hash is calend; returning nothing\n";
        return credit_hashes;
    }

    credit_hashes.push_back(mined_credit.GetHash160());

    while (mined_credit.batch_number && 
            mined_credit.previous_mined_credit_hash != calend_credit_hash)
    {
        uint160 prev_hash = mined_credit.previous_mined_credit_hash;
        if (prev_hash == 0)
            break;
        credit_hashes.push_back(prev_hash);
        mined_credit = creditdata[prev_hash]["mined_credit"];
    }
    reverse(credit_hashes.begin(), credit_hashes.end());
    log_ << "GetCreditHashesSinceCalend: returning: " << credit_hashes << "\n";
    return credit_hashes;
}

uint160 PreviousCreditHash(uint160 credit_hash)
{
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
    return mined_credit.previous_mined_credit_hash;
}

vector<Credit> GetCreditsFromHashes(vector<uint160> hashes)
{
    vector<Credit> credits;
    for (uint32_t i = 0; i < hashes.size(); i++)
    {
        if (creditdata[hashes[i]].HasProperty("tx"))
        {
            SignedTransaction tx = creditdata[hashes[i]]["tx"];
            for (uint32_t j = 0; j < tx.rawtx.outputs.size(); j++)
                credits.push_back(tx.rawtx.outputs[j]);
        }
        else if (creditdata[hashes[i]].HasProperty("mined_credit"))
        {
            MinedCredit mined_credit = creditdata[hashes[i]]["mined_credit"];
            credits.push_back((Credit)mined_credit);
        }
    }
    return credits;
}

vector<Credit> GetCreditsFromMsg(MinedCreditMessage& msg)
{
    msg.hash_list.RecoverFullHashes();
    return GetCreditsFromHashes(msg.hash_list.full_hashes);
}

vector<uint64_t> GetBitsSetByTxs(vector<uint160> txs)
{
    vector<uint64_t> positions;
    for (uint32_t i = 0; i < txs.size(); i++)
    {
        if (!creditdata[txs[i]].HasProperty("tx"))
            continue;
        SignedTransaction tx = creditdata[txs[i]]["tx"];
        for (uint32_t j = 0; j < tx.rawtx.inputs.size(); j++)
        {
            uint64_t position = tx.rawtx.inputs[j].position;
            positions.push_back(position);
        }
    }
    return positions;
}

vector<uint64_t> GetBitsSetInBatch(uint160 credit_hash)
{
    vector<uint160> txs = batchhashesdb[credit_hash];
    return GetBitsSetByTxs(txs);
}

bool SeenBefore(uint160 credit_hash)
{
    return creditdata[credit_hash].HasProperty("mined_credit");
}


uint160 GetFork(uint160 hash1, uint160 hash2, uint32_t& stepsback)
{
    stepsback = 0;

    if (hash1 == hash2)
        return hash1;

    set<uint160> seen;
    uint160 fork = 0;
    while (true)
    {
        MinedCredit mined_credit1 = creditdata[hash1]["mined_credit"];
        MinedCredit mined_credit2 = creditdata[hash2]["mined_credit"];

        if (hash1 > 0) seen.insert(hash1);
        if (hash2 > 0) seen.insert(hash2);

        if (mined_credit1.previous_mined_credit_hash == 0 &&
            mined_credit2.previous_mined_credit_hash == 0)
            break;

        if (mined_credit1.previous_mined_credit_hash != 0)
            stepsback++;

        if (mined_credit1.previous_mined_credit_hash != 0
            && seen.count(mined_credit1.previous_mined_credit_hash))
            return mined_credit1.previous_mined_credit_hash;
        if (mined_credit2.previous_mined_credit_hash != 0
            && seen.count(mined_credit2.previous_mined_credit_hash))
            return mined_credit2.previous_mined_credit_hash;
        hash1 = mined_credit1.previous_mined_credit_hash;
        hash2 = mined_credit2.previous_mined_credit_hash;

    }
    return fork;
}

bool GetSpentChainDifferences(uint160 starting_credit_hash, 
                              uint160 target_credit_hash, 
                              vector<uint64_t> &to_set,
                              vector<uint64_t> &to_clear)
{
    uint32_t n;
    uint160 fork = GetFork(starting_credit_hash, target_credit_hash, n);
    log_ << "GetSpentChainDifferences: from " << starting_credit_hash
         << " to " << target_credit_hash << "\n";
    log_ << "fork is " << fork << "\n";
    to_set.resize(0); to_clear.resize(0);

    uint160 hash = target_credit_hash;
    uint32_t look_back = 0;
 
    while (hash != fork)
    {
        vector<uint64_t> spent = GetBitsSetInBatch(hash);
        to_set.insert(to_set.end(), spent.begin(), spent.end());
        log_ << "adding " << spent << "to to_set\n";
        hash = PreviousCreditHash(hash);
 
        look_back++;

        if (look_back > 300)
        {
            to_set.resize(0);
            log_ << "GetSpentChainDifferences(): lookback > 300\n";
            return false;
        }
    }
    
    // backward from most recent mined credit to fork
    uint160 hash_ = starting_credit_hash;
    
    while (hash_ != fork)
    {
        vector<uint64_t> spent = GetBitsSetInBatch(hash_);

        to_clear.insert(to_clear.end(), spent.begin(), spent.end());

        hash_ = PreviousCreditHash(hash_);
    
        look_back++;

        if (look_back > 300)
        {
            to_clear.resize(0);
            log_ << "GetSpentChainDifferences(): lookback > 300\n";
            return false;
        }
    }

    for (uint32_t i = 0; i < to_set.size(); i++)
        for (uint32_t j = 0; j < to_clear.size(); j++)
            if (to_set[i] == to_clear[j])
            {
                to_set.erase(to_set.begin() + i);
                to_clear.erase(to_clear.begin() + j);
                i--;
                break;
            }
    log_ << "GetSpentChainDifferences: from "
         << starting_credit_hash << " to " << target_credit_hash << "\n"
         << "to set = " << to_set << "   to clear = " << to_clear << "\n";

    return true;
}


bool HaveSameCalend(uint160 credit_hash1, uint160 credit_hash2)
{
    uint160 previous_diurn_root1 = 0, previous_diurn_root2 = 0;
    
    MinedCredit mined_credit1 = creditdata[credit_hash1]["mined_credit"];
    MinedCredit mined_credit2 = creditdata[credit_hash2]["mined_credit"];
    
    previous_diurn_root1 = mined_credit1.previous_diurn_root;
    previous_diurn_root2 = mined_credit2.previous_diurn_root;

    return previous_diurn_root1 == previous_diurn_root2;
}

bool TxContainsSpentInputs(SignedTransaction tx)
{
    foreach_(const CreditInBatch& credit, tx.rawtx.inputs)
        if (flexnode.spent_chain.Get(credit.position))
            return true;
    return false;
}

bool IsCalend(uint160 credit_hash)
{
    if (!creditdata[credit_hash].HasProperty("msg"))
        return false;
    if (calendardata[credit_hash]["is_calend"])
        return true;
    MinedCreditMessage msg = creditdata[credit_hash]["msg"];
    if (msg.proof.DifficultyAchieved() > msg.mined_credit.diurnal_difficulty)
    {
        calendardata[credit_hash]["is_calend"] = true;
    }
    return calendardata[credit_hash]["is_calend"];
}

CreditBatch ReconstructBatch(MinedCreditMessage& msg)
{
    CreditBatch credit_batch(msg.mined_credit.previous_mined_credit_hash,
                             msg.mined_credit.offset);
    msg.hash_list.RecoverFullHashes();
    vector<Credit> credits = GetCreditsFromHashes(msg.hash_list.full_hashes);
    
    uint160 prev_hash = msg.mined_credit.previous_mined_credit_hash;

    if (IsCalend(prev_hash))
    {
        RelayState relay_state = GetRelayState(prev_hash);
        vector<Credit> fees = GetFeesOwed(relay_state);
        credits.insert(credits.end(), fees.begin(), fees.end());
    }
    for (uint32_t i = 0; i < credits.size(); i++)
    {
        credit_batch.Add(credits[i]);
    }
    return credit_batch;
}

int64_t GetFeeSize(SignedTransaction tx)
{
    int64_t in = 0, out = 0;
    foreach_(const Credit& output, tx.rawtx.outputs)
        out += output.amount;
    foreach_(const CreditInBatch& input, tx.rawtx.inputs)
        in += input.amount;
    return in - out;
}

bool PreviousBatchWasCalend(MinedCreditMessage& msg)
{
    uint160 prev_hash = msg.mined_credit.previous_mined_credit_hash;
    if (calendardata[prev_hash]["is_calend"])
        return true;
    return false;
}

uint64_t SpentChainLength(uint160 mined_credit_hash)
{
    uint64_t length = 0;
    MinedCreditMessage msg = creditdata[mined_credit_hash]["msg"];
    log_ << "Spentchainlength: msg is " << msg;
    log_ << "GetCreditsFromMsg(msg) is " << GetCreditsFromMsg(msg) << "\n";
    length = msg.mined_credit.offset + GetCreditsFromMsg(msg).size();

    uint160 previous_hash = msg.mined_credit.previous_mined_credit_hash;
    if (IsCalend(previous_hash))
    {
        RelayState state = GetRelayState(previous_hash);
        length += GetFeesOwed(state).size();
    }
    return length;
}

bool ChangeSpentChain(BitChain *spent_chain_,
                      uint160 previous_credit_hash,
                      uint160 target_credit_hash)
{
    vector<uint64_t> to_set, to_clear;
    if (!GetSpentChainDifferences(previous_credit_hash,
                                  target_credit_hash,
                                  to_set, to_clear))
        return false;

    uint64_t length = SpentChainLength(target_credit_hash);
    log_ << "ChangeSpentChain: new length is " << length << "\n";

    log_ << "to_set is " << to_set << "\n";
    if (length == 0)
        return false;

    spent_chain_->SetLength(length);

    foreach_(const uint64_t& bit, to_set)
        spent_chain_->Set(bit);
    foreach_(const uint64_t& bit, to_clear)
        spent_chain_->Clear(bit);
    return true;
}

BitChain GetSpentChain(uint160 credit_hash)
{
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];

    while (!creditdata[mined_credit.spent_chain_hash].HasProperty("chain"))
    {
        uint160 prev_hash = mined_credit.previous_mined_credit_hash;
        mined_credit = creditdata[prev_hash]["mined_credit"];
        if (!mined_credit.spent_chain_hash)
            break;
    }
    uint160 chain_hash = mined_credit.spent_chain_hash;
    BitChain spentchain = creditdata[chain_hash]["chain"];

    log_ << "GetSpentChain: got a chain for " << mined_credit
         << "namely " << spentchain << "\n";
    
    log_ << "changing to spent chain for " << credit_hash << "\n";
    ChangeSpentChain(&spentchain, mined_credit.GetHash160(), credit_hash);

    log_ << "GetSpentChain: chain is now " << spentchain << "\n";
    return spentchain;
}

bool TransactionsConflict(SignedTransaction tx1, SignedTransaction tx2)
{
    for (uint32_t j = 0; j < tx1.rawtx.inputs.size(); j++)
        for (uint32_t k = 0; k < tx2.rawtx.inputs.size(); k++)
            if (tx1.rawtx.inputs[k] == tx2.rawtx.inputs[j])
                return true;
    return false;
}


uint160 GetNextCalendCreditHash(uint160 credit_hash)
{
    std::vector<uint160> calend_hashes;
    calend_hashes = flexnode.calendar.GetCalendCreditHashes();
    if (!VectorContainsEntry(calend_hashes, credit_hash)
        || credit_hash == calend_hashes.back())
        return 0;

    for (uint32_t i = 0; i < calend_hashes.size(); i++)
        if (calend_hashes[i] == credit_hash)
            return calend_hashes[i + 1];

    return 0;
}

bool GetTxDifferences(uint160 previous_credit_hash, 
                      uint160 target_credit_hash, 
                      vector<uint160> &to_add,
                      vector<uint160> &to_delete)
{
    to_add.resize(0); to_delete.resize(0);
    log_ << "GetTxDifferences: from " << previous_credit_hash << " to "
         << target_credit_hash << "\n";
    uint160 hash = target_credit_hash;
    
    while (!InMainChain(hash) && hash != 0)
    {
        vector<uint160> contents = batchhashesdb[hash];
        to_add.insert(to_add.end(), contents.begin(), contents.end());
        hash = PreviousCreditHash(hash);
    }

    // backward from most recent batch to fork
    uint160 hash_ = previous_credit_hash;
    while (hash_ != hash && hash_ != 0)
    {
        vector<uint160> contents = batchhashesdb[hash_];
        to_delete.insert(to_delete.end(), contents.begin(), contents.end());

        MinedCredit mined_credit = creditdata[hash_]["mined_credit"];
        hash_ = mined_credit.previous_mined_credit_hash;
    }
    for (uint32_t i = 0; i < to_add.size(); i++)
        for (uint32_t j = 0; j < to_delete.size(); j++)
            if (to_add[i] == to_delete[j])
            {
                to_add.erase(to_add.begin() + i);
                to_delete.erase(to_delete.begin() + j);
                i--;
                break;
            }
    return true;
}

uint64_t TransactionDepth(uint160 txhash, uint160 current_credit_hash)
{
    uint160 mined_credit_hash = creditdata[txhash]["credit_hash"];
    MinedCredit mined_credit = creditdata[mined_credit_hash]["mined_credit"];
    MinedCredit current_credit
        = creditdata[current_credit_hash]["mined_credit"];
    return current_credit.batch_number - mined_credit.batch_number;
}

MinedCredit LatestMinedCredit()
{
    MinedCredit mined_credit
        = creditdata[flexnode.previous_mined_credit_hash]["mined_credit"];
    return mined_credit;
}

void SetChildBlock(uint160 parent_hash, uint160 child_hash)
{
    vector<uint160> children = creditdata[parent_hash]["children"];
    if (!VectorContainsEntry(children, child_hash))
        children.push_back(child_hash);
    creditdata[parent_hash]["children"] = children;
}

void RecordBatch(const MinedCreditMessage& msg)
{
    MinedCreditMessage msg_ = msg;
    uint160 credit_hash = msg_.mined_credit.GetHash160();
    
    msgdata[credit_hash]["received"] = true;
    msgdata[msg_.mined_credit.GetHash()]["received"] = true;
    msgdata[msg_.GetHash()]["received"] = true;
    
    msg_.hash_list.RecoverFullHashes();
    batchhashesdb[credit_hash] = msg_.hash_list.full_hashes;
    creditdata[credit_hash]["msg"] = msg_;
    creditdata[credit_hash]["mined_credit"] = msg_.mined_credit;
    
    SetChildBlock(msg_.mined_credit.previous_mined_credit_hash, credit_hash);

    workdata[msg_.proof.GetHash()]["proof"] = msg.proof;
}

void UpdateSpentChain(UnsignedTransaction tx)
{
    for (uint32_t i = 0; i < tx.inputs.size(); i++)
    {
        log_ << "UpdateSpentChain: setting position " 
             << tx.inputs[i].position << "\n";

        flexnode.spent_chain.Set(tx.inputs[i].position);
    }
    
    for (uint32_t i = 0; i < tx.outputs.size(); i++)
        flexnode.spent_chain.Add();
}

string string_to_hex(const string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();

    string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

vector<uint160> MissingDataNeededToCalculateFees(RelayState state)
{
    vector<uint160> needed;
    foreach_(HashToPointMap::value_type item, state.join_hash_to_relay)
    {
        uint160 join_msg_hash = item.first;
    
        if (!msgdata[join_msg_hash].HasProperty("join"))
        {
            needed.push_back(join_msg_hash);
            continue;
        }
        RelayJoinMessage join = msgdata[join_msg_hash]["join"];
        
        if (!creditdata[join.credit_hash].HasProperty("mined_credit"))
        {
            needed.push_back(join_msg_hash);
            continue;
        }
    }
    log_ << "MissingDataNeededToCalculateFees: " << needed << "\n";
    return needed;
}

vector<Credit> GetFeesOwed(RelayState state)
{
    vector<Credit> fees_owed;
    foreach_(const Numbering::value_type& item, state.relays)
    {
        Point relay = item.first;
        log_ << "GetFeesOwed: considering: " << relay << "\n";

        if (!state.disqualifications.count(relay) &&
            !state.actual_successions.count(relay))
        {
            uint160 join_msg_hash = state.GetJoinHash(relay);
            log_ << "GetFeesOwed: join hash is " << join_msg_hash << "\n";
            RelayJoinMessage join = msgdata[join_msg_hash]["join"];
            log_ << "join is " << join << "\n";
            Point mined_credit_key = join.VerificationKey();
            log_ << "GetFeesOwed(): key is " << mined_credit_key << "\n";
            if (!creditdata[join.credit_hash].HasProperty("mined_credit"))
            {
                throw MissingDataException("mined_credit: "
                                           + join.credit_hash.ToString());
            }
            log_ << "GetFeesOwed(): key is mine: " 
                 << keydata[mined_credit_key].HasProperty("privkey")
                 << "\n";
            Credit fee(mined_credit_key.getvch(), RELAY_FEE);
            fees_owed.push_back(fee);
            log_ << "added fee of " << RELAY_FEE << " with keydata: "
                 << fee.keydata << "\n";
        }
        else
        {
            log_ << "No payment: disqualification: "
                 << state.disqualifications.count(relay) 
                 << " succession: " 
                 << state.actual_successions.count(relay) << "\n";
        }
    }
    std::sort(fees_owed.begin(), fees_owed.end());
    return fees_owed;
}

void StoreHash(uint160 hash)
{
    uint32_t short_hash = *(uint32_t*)&hash;

    vector<uint160> matches = hashdata[short_hash]["matches"];
    if (!VectorContainsEntry(matches, hash))
        matches.push_back(hash);
    hashdata[short_hash]["matches"] = matches;
}

void StoreMinedCredit(MinedCredit mined_credit)
{
    uint160 hash = mined_credit.GetHash160();
    StoreHash(hash);
    msgdata[hash]["type"] = string_t("mined_credit");
    creditdata[hash]["mined_credit"] = mined_credit;
}

void StoreMinedCreditMessage(MinedCreditMessage msg)
{
    uint160 hash = msg.GetHash160();
    uint160 mined_credit_hash = msg.mined_credit.GetHash160();
    StoreHash(hash);
    creditdata[hash]["msg"] = msg;
    creditdata[mined_credit_hash]["msg"] = msg;
    StoreMinedCredit(msg.mined_credit);
}

void StoreTransaction(SignedTransaction tx)
{
    uint160 hash = tx.GetHash160();
    StoreHash(hash);
    msgdata[hash]["type"] = string_t("tx");
    creditdata[hash]["tx"] = tx;
}

class TimeAndRecipient
{
public:
    uint256 data;

    TimeAndRecipient() { }

    TimeAndRecipient(uint64_t time_, uint160 recipient)
    {
        data = time_;
        data <<= 160;
        memcpy((char*)&data, (char*)&recipient, 20);
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(data);
    )

    uint64_t Time()
    {
        uint256 data_ = data;
        data_ >>= 160;
        return data_.GetLow64();
    }

    uint160 Recipient()
    {
        uint160 recipient;
        memcpy((char*)&recipient, (char*)&data, 20);
        return recipient;
    }
};

uint32_t GetStub(uint160 hash)
{
    return hash.GetLow64() % (256 * 256 * 256);
}

void StoreRecipients(CreditBatch& batch)
{
    uint160 batch_root = batch.Root();
    uint64_t now = GetTimeMicros();

    uint64_t n = 0;
    foreach_(Credit& credit, batch.credits)
    {
        uint160 keyhash;

        if (credit.keydata.size() == 20)
            keyhash = uint160(credit.keydata);
        else
            keyhash = KeyHashFromKeyData(credit.keydata);

        pair<uint160, uint64_t> batch_entry(batch_root, n);
        uint32_t stub = GetStub(keyhash);
        creditdata[batch_entry].Location(stub) = TimeAndRecipient(now, 
                                                                  keyhash);
        n += 1;
    }
}

uint160 KeyHashFromKeyData(vch_t keydata)
{
    if (keydata.size() == 20)
        return uint160(keydata);
    else
    {
        Point pubkey;
        pubkey.setvch(keydata);
        return KeyHash(pubkey);
    }
}

CreditInBatch GetCreditFromBatch(uint160 batch_root, uint32_t n)
{
    uint160 credit_hash = creditdata[batch_root]["credit_hash"];
    MinedCreditMessage msg = creditdata[credit_hash]["msg"];
    CreditBatch batch = ReconstructBatch(msg);
    if (n >= batch.credits.size())
        return CreditInBatch();
    Credit credit = batch.credits[n];
    CreditInBatch credit_in_batch = batch.GetWithPosition(credit);
    flexnode.wallet.AddDiurnBranch(credit_in_batch);
    return credit_in_batch;
}

vector<CreditInBatch> GetCreditsPayingToRecipient(vector<uint32_t> stubs,
                                                  uint64_t since_when)
{
    vector<CreditInBatch> credits;

    foreach_(uint32_t stub, stubs)
    {
        CLocationIterator<uint32_t> credit_scanner;
        credit_scanner = creditdata.LocationIterator(stub);

        credit_scanner.Seek(TimeAndRecipient(since_when, 0));

        TimeAndRecipient time_and_recipient;
        pair<uint160,uint64_t> batch_entry;

        while (credit_scanner.GetNextObjectAndLocation(batch_entry,
                                                       time_and_recipient))
        {
            uint160 batch_root = batch_entry.first;
            uint32_t n = batch_entry.second;
            uint160 recipient = time_and_recipient.Recipient();

            if (VectorContainsEntry(stubs, GetStub(recipient)))
                credits.push_back(GetCreditFromBatch(batch_root, n));
        }
    }
    return credits;
}

vector<CreditInBatch> GetCreditsPayingToRecipient(uint160 keyhash)
{
    vector<CreditInBatch> credits;

    uint32_t stub = GetStub(keyhash);
    vector<uint32_t> stubs;
    stubs.push_back(stub);
    vector<CreditInBatch> recorded_credits;
    recorded_credits = GetCreditsPayingToRecipient(stubs, 0);
    
    foreach_(CreditInBatch credit, recorded_credits)
    {
        uint160 recipient_keyhash;
        if (credit.keydata.size() == 20)
            recipient_keyhash = uint160(credit.keydata);
        else
        {
            Point recipient;
            recipient.setvch(credit.keydata);
            recipient_keyhash = KeyHash(recipient);
        }
        if (recipient_keyhash == keyhash)
            credits.push_back(credit);
    }
    return credits;
}

uint64_t GetKnownPubKeyBalance(Point pubkey)
{
    uint64_t balance = 0;
    uint32_t stub = GetStub(KeyHash(pubkey));
    vector<uint32_t> stubs;
    stubs.push_back(stub);
    vector<CreditInBatch> credits = GetCreditsPayingToRecipient(stubs, 0);

    foreach_(CreditInBatch credit, credits)
    {
        if (!flexnode.spent_chain.Get(credit.position))
            balance += credit.amount;
    }
    return balance;
}

void RemoveCreditsOlderThan(uint64_t time_, uint32_t stub)
{
    CLocationIterator<uint32_t> credit_scanner;
    credit_scanner = creditdata.LocationIterator(stub);

    credit_scanner.Seek(TimeAndRecipient(time_, 0));

    TimeAndRecipient time_and_recipient;
    pair<uint160,uint64_t> batch_entry;

    while (credit_scanner.GetPreviousObjectAndLocation(batch_entry, 
                                                       time_and_recipient))
    {
        creditdata.RemoveFromLocation(stub, time_and_recipient);
        credit_scanner = creditdata.LocationIterator(stub);
        credit_scanner.Seek(TimeAndRecipient(time_, 0));
    }
}

uint160 GetNextDifficulty(MinedCredit credit)
{
    if (credit.difficulty == 0)
        return INITIAL_DIFFICULTY;
    uint160 prev_hash = credit.previous_mined_credit_hash;
    MinedCredit preceding_credit = creditdata[prev_hash]["mined_credit"];
    uint160 next_difficulty
              = credit.timestamp - preceding_credit.timestamp > 6e7
                ? (credit.difficulty * 95) / uint160(100)
                : (credit.difficulty * 100) / uint160(95);
    return next_difficulty;
}

uint160 GetNextDifficulty(uint160 mined_credit_hash)
{
    MinedCredit mined_credit = creditdata[mined_credit_hash]["mined_credit"];
    return GetNextDifficulty(mined_credit);
}

void RemoveBlocksAndChildren(uint160 credit_hash)
{
    creditdata[credit_hash].Location("total_work") = 0;
    RemoveFromMainChain(credit_hash);
    vector<uint160> children = creditdata[credit_hash]["children"];
    foreach_(const uint160& child, children)
        RemoveBlocksAndChildren(child);
}

uint160 GetReportedWork(uint160 credit_hash)
{
    MinedCredit credit = creditdata[credit_hash]["mined_credit"];
    return credit.total_work + credit.difficulty;
}

uint160 GetLastHash(CDataStore& datastore, string_t dimension)
{
    CLocationIterator<> scanner = datastore.LocationIterator(dimension);
    scanner.SeekEnd();
    uint160 location;
    uint160 hash;
    scanner.GetPreviousObjectAndLocation(hash, location);
    return hash;
}

void AddToMainChain(uint160 credit_hash)
{
    creditdata[credit_hash].Location("main_chain")
        = GetReportedWork(credit_hash);
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
    creditdata[mined_credit.batch_root]["credit_hash"] = credit_hash;
}

void RemoveFromMainChain(uint160 credit_hash)
{
    log_ << "removing " << credit_hash << " from main chain\n";
    creditdata.RemoveFromLocation("main_chain", GetReportedWork(credit_hash));
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
    creditdata[mined_credit.batch_root]["credit_hash"] = 0;
}

void ClearMainChain()
{
    uint160 hash = 1;

    while (hash != 0)
    {
        hash = GetLastHash(creditdata, "main_chain");
        RemoveFromMainChain(hash);
    }
}

bool InMainChain(uint160 credit_hash)
{
    uint160 location = creditdata[credit_hash].Location("main_chain");
    return location != 0;
}
uint160 MostWorkBatch()
{
    return GetLastHash(creditdata, "total_work");
}

uint160 MostReportedWorkBatch()
{
    return GetLastHash(initdata, "reported_work");
}

CNode *GetPeer(uint160 message_hash)
{
    int32_t node_id = msgdata[message_hash]["peer"];
    CNode *peer = NULL;
    foreach_(CNode* pnode, vNodes)
    {
        if(pnode->id == node_id)
            peer = pnode;
    }
    return peer;
}

void FetchDependencies(std::vector<uint160> dependencies, CNode *peer)
{
    std::vector<uint160> unreceived_dependencies;

    foreach_(uint160 dependency, dependencies)
        if (!msgdata[dependency]["received"])
            unreceived_dependencies.push_back(dependency);

    if (peer != NULL)
    {
        std::vector<uint160> empty;
        flexnode.downloader.datahandler.SendDataRequest(
                                                empty,
                                                unreceived_dependencies,
                                                empty,
                                                std::vector<Point>(),
                                                peer);
    }
}

string_t HistoryString(uint160 credit_hash)
{
    uint160 total_work(0);

    stringstream ss;
    ss << "\n============== Known History =============" << "\n"
       << "== Final Credit Hash: " << credit_hash.ToString() << "\n"
       << "==" << "\n";
    while (credit_hash != 0)
    {
        MinedCredit credit = creditdata[credit_hash]["mined_credit"];
        ss << "== " << credit_hash.ToString() 
           << " (" << credit.difficulty.ToString() << ")" << "\n";
        credit_hash = credit.previous_mined_credit_hash;
        total_work += credit.difficulty;
    }
    ss << "== Total Work: " << total_work.ToString() << "\n"
       << "============ End Known History ===========" << "\n";
    return ss.str();
}

