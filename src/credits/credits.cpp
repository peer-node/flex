// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "flexnode/flexnode.h"

using namespace std;

#include "log.h"
#define LOG_CATEGORY "credits.cpp"


bool CheckDifficulty(MinedCreditMessage& msg)
{
    uint160 prev_credit_hash = msg.mined_credit.previous_mined_credit_hash;
    MinedCredit prev_credit = creditdata[prev_credit_hash]["mined_credit"];
    uint160 next_difficulty = GetNextDifficulty(prev_credit);
    bool result = msg.mined_credit.difficulty == next_difficulty;
    if (!result)
        log_ << "CheckDifficulty failed! "
             << msg.mined_credit.difficulty << " < " 
             << next_difficulty << "\n";
    return result;
}

bool CheckMessageProof(MinedCreditMessage msg)
{
    log_ << "CheckMessageProof()\n";
    MinedCredit credit = msg.mined_credit;

    uint160 batch_difficulty = msg.mined_credit.difficulty;
    
    if (batch_difficulty == 0)
    {
        log_ << "batch_difficulty = 0\n";
        return false;
    }

    uint160 nugget_difficulty = batch_difficulty / NUGGETS_PER_BATCH;
    uint160 numerator = 1;
    numerator = numerator << 128;
    uint160 nugget_target = numerator / nugget_difficulty;

    if (msg.proof.memoryseed != msg.mined_credit.GetHash())
    {
        log_ << "bad memory seed: " 
             << msg.proof.memoryseed << " vs" 
             << msg.mined_credit << "\n"
             << "mined credit is: " << msg.mined_credit << "\n";
        return false;
    }

    if (msg.proof.N_factor == BLOCK_MEMORY_FACTOR &&
        msg.proof.num_segments == SEGMENTS_PER_PROOF &&
        msg.proof.target == nugget_target &&
        msg.proof.link_threshold / LINKS_PER_PROOF == msg.proof.target &&
        msg.proof.DifficultyAchieved() >= nugget_difficulty)
    {
        log_ << "CheckMessageProof(): returning true\n";
        return true;
    }
    else
    {
        log_ << "CheckMessageProof failed\n"
             << "nNfactor = " << msg.proof.N_factor << " vs " 
             << BLOCK_MEMORY_FACTOR
             << "target = " << msg.proof.target << " vs " << nugget_target
             << "target = " << msg.proof.target << " vs " 
             << (msg.proof.link_threshold / LINKS_PER_PROOF)
             << "link_threshold = " <<  msg.proof.link_threshold
             << "difficulty is "
             << msg.proof.DifficultyAchieved()  << " vs "
             << nugget_difficulty << "\n";
    }
    return false;
}

bool CheckNuggetSpentChainRoot(MinedCreditMessage& msg)
{
    vector<uint160> hashes = msg.hash_list.full_hashes;
    MinedCredit mined_credit = msg.mined_credit;

    uint160 credit_hash = mined_credit.GetHash160();
    RecordBatch(msg);

    BitChain spent_chain = GetSpentChain(credit_hash);
    uint160 chain_hash = spent_chain.GetHash160();

    bool result = chain_hash == mined_credit.spent_chain_hash;

    log_ << "checknuggetspentroot: " << result << "\n"
         << chain_hash << " vs " << mined_credit.spent_chain_hash << "\n";

    return result;
}

bool CheckNuggetBatchRoot(MinedCreditMessage& msg)
{
    CreditBatch credit_batch = ReconstructBatch(msg);
    uint160 root = credit_batch.Root();
    bool result = msg.mined_credit.batch_root == root;
    
    log_ << "checknuggetbatchroot: " << result << "\n"
         << root << " vs " << msg.mined_credit.batch_root << "\n"
         << "credit_batch is " <<  credit_batch << "\n";
    return result;
}

bool CheckNuggetRoots(MinedCreditMessage& msg)
{
    return CheckNuggetBatchRoot(msg) && CheckNuggetSpentChainRoot(msg);
}

bool CheckNuggetIsIncludable(uint160 nugget_hash,
                             uint160 preceding_credit_hash)
{
    // includable if
    // 1) nugget's parent is the specified preceding credit
    MinedCredit nugget = creditdata[nugget_hash]["mined_credit"];
    if (nugget.previous_mined_credit_hash == preceding_credit_hash)
        return true;
    else
    {
        log_ << "nugget's parent " << nugget.previous_mined_credit_hash
             << " != preceding credit hash " 
             << preceding_credit_hash << "\n";
    }

    // or
    // 2 a) nugget's came from block before that and
    uint160 nugget_parent_hash = nugget.previous_mined_credit_hash;
    MinedCredit preceding_credit
        = creditdata[preceding_credit_hash]["mined_credit"];

    if (preceding_credit.previous_mined_credit_hash != nugget_parent_hash)
        return false;

    log_ << "nugget's parent " << nugget_parent_hash
         << " == antepreceding credit hash "
         << preceding_credit.previous_mined_credit_hash << "\n";


    // 2 b) wasn't included in that block
    vector<uint160> included = batchhashesdb[nugget_parent_hash];
    if (VectorContainsEntry(included, nugget_hash))
    {
        log_ << "nugget was included in block "
             << nugget_parent_hash << "!\n";
        return false;
    }
    return true;
}

bool CheckNuggets(MinedCreditMessage& msg)
{
    uint160 prev_hash = msg.mined_credit.previous_mined_credit_hash;
    foreach_(uint160 hash, msg.hash_list.full_hashes)
    {
        if (creditdata[hash].HasProperty("mined_credit") &&
            !CheckNuggetIsIncludable(hash, prev_hash))
            return false;        
    }
    return true;
}

bool CheckTxWithBitDifferences(uint160 txhash, 
                               vector<uint64_t> bits_to_set,
                               vector<uint64_t> bits_to_clear)
{
    SignedTransaction tx = creditdata[txhash]["tx"];

    for (uint32_t i = 0; i < tx.rawtx.inputs.size(); i++)
    {
        uint64_t position = tx.rawtx.inputs[i].position;
        if (flexnode.spent_chain.Get(position)
            && !VectorContainsEntry(bits_to_clear, position))
            return false;
        else if (VectorContainsEntry(bits_to_set, position))
            return false;
    }
    return true;
}     

bool CheckTransactions(MinedCreditMessage& msg)
{
    vector<uint64_t> bits_to_set;
    vector<uint64_t> bits_to_clear;

    if (!GetSpentChainDifferences(flexnode.previous_mined_credit_hash, 
                                  msg.mined_credit.previous_mined_credit_hash,
                                  bits_to_set, bits_to_clear))
        return false;

    for (uint32_t i = 0; i < msg.hash_list.full_hashes.size(); i++)
    {
        uint160 hash = msg.hash_list.full_hashes[i];
        if (creditdata[hash].HasProperty("tx"))
            if (!CheckTxWithBitDifferences(hash, bits_to_set, bits_to_clear))
                return false;
    }
    return true;
}

bool CheckDiurnalDifficulty(MinedCreditMessage& msg)
{
    log_ << "CheckDiurnalDifficulty(): " << msg.mined_credit;

    uint160 prev_credit_hash = msg.mined_credit.previous_mined_credit_hash;

    if (prev_credit_hash == 0)
        return msg.mined_credit.diurnal_difficulty
                == INITIAL_DIURNAL_DIFFICULTY;

    MinedCreditMessage prev_msg = creditdata[prev_credit_hash]["msg"];

    log_ << "previous credit: " << prev_msg.mined_credit;

    uint160 correct_diurnal_difficulty;

    log_ << "proof length is: " << prev_msg.proof.Length() << "\n";
    log_ << "difficulty achieved: " << prev_msg.proof.DifficultyAchieved()
         << "\n";
    log_ << "diurnal_difficulty: " << prev_msg.mined_credit.diurnal_difficulty
         << "\n";

    if (prev_msg.proof.Length() > 0 &&
        prev_msg.proof.DifficultyAchieved()
            > prev_msg.mined_credit.diurnal_difficulty)
    {
        log_ << "previous credit was calend\n";
        
        uint160 calend_hash = GetCalendCreditHash(prev_credit_hash);

        MinedCredit calend_credit = creditdata[calend_hash]["mined_credit"];
        
        correct_diurnal_difficulty
            = GetNextDiurnInitialDifficulty(calend_credit,
                                            prev_msg.mined_credit);
        log_ << "next diurnal difficulty is "
             << correct_diurnal_difficulty << "\n";
    }
    else
    {
        correct_diurnal_difficulty
            = ApplyAdjustments(prev_msg.mined_credit.diurnal_difficulty, 1);
        
        log_ << "not a calend: diurnal difficulty is: "
             << correct_diurnal_difficulty << "\n";
    }

    if (msg.mined_credit.diurnal_difficulty != correct_diurnal_difficulty)
    {
        log_ << "CheckDiurnalDifficulty failed:\n"
             << msg.mined_credit.diurnal_difficulty << " vs "
             << correct_diurnal_difficulty << "\n";
        return false;
    }

    return true;
}


bool CheckRelayState(MinedCreditMessage& msg)
{
    uint160 credit_hash = msg.mined_credit.previous_mined_credit_hash;
    RelayState state = GetRelayState(credit_hash);

    RelayStateChain state_chain(state);
    log_ << "CheckRelayState: checking state of " 
         << msg.mined_credit << "\n";

    if (!state_chain.ValidateBatch(msg.mined_credit.GetHash160()))
    {
        log_ << "validatebatch failed!\n";
        return false;
    }

    MinedCredit prev_mined_credit = creditdata[credit_hash]["mined_credit"];
    
    msg.hash_list.RecoverFullHashes();
    foreach_(const uint160 message_hash, msg.hash_list.full_hashes)
    {
        log_ << "msg contains: " << message_hash << "\n";
    }
    foreach_(const uint160 message_hash, msg.hash_list.full_hashes)
    {
        log_ << "applying " << message_hash << "\n";
        state_chain.HandleMessage(message_hash);
    }

    state_chain.state.latest_credit_hash
        = msg.mined_credit.previous_mined_credit_hash;

    bool result = state_chain.GetHash160() == msg.mined_credit.relay_state_hash;
    log_ << "CheckRelayState: " << state_chain.GetHash160() << " vs "
         << msg.mined_credit.relay_state_hash << "\n";
    return result;
}





/*******************
 *  CreditInBatch
 */

    CreditInBatch::CreditInBatch(Credit credit, uint64_t position, 
                  std::vector<uint160> branch):
        position(position),
        branch(branch)
    {
        keydata = credit.keydata;
        amount = credit.amount;
    }

    string_t CreditInBatch::ToString() const
    {
        stringstream ss;
        ss << "\n============== CreditInBatch =============" << "\n"
           << "== Position: " << position << "\n"
           << "== \n"
           << "== Branch:\n";
        foreach_(uint160 hash, branch)
            ss << "== " << hash.ToString() << "\n";
        ss << "== \n"
           << "== Diurn Branch:\n";
        foreach_(uint160 hash, diurn_branch)
            ss << "== " << hash.ToString() << "\n";
        ss << "==" << "\n"
           << "== Credit: " << Credit(keydata, amount).ToString() << "\n"
           << "============ End CreditInBatch ===========" << "\n";
        return ss.str();
    }

    vch_t CreditInBatch::getvch()
    {
        vch_t result;
        result.resize(12 + keydata.size() + 20 * branch.size());
        
        memcpy(&result[0], &amount, 8);
        memcpy(&result[8], &keydata[0], keydata.size());
        memcpy(&result[8 + keydata.size()], &position, 4);
        memcpy(&result[12 + keydata.size()], &branch[0], 20 * branch.size());
        
        return result;
    }   

    bool CreditInBatch::operator==(const CreditInBatch& other)
    {
        return branch == other.branch && position == other.position;
    }

    bool CreditInBatch::operator<(const CreditInBatch& other) const
    {
        return amount < other.amount;
    }

/*
 *  CreditInBatch
 *******************/


/*****************
 *  CreditBatch
 */

    CreditBatch::CreditBatch()
    {
        tree.offset = 0ULL;
    }

    CreditBatch::CreditBatch(uint160 previous_credit_hash, uint64_t offset)
    : previous_credit_hash(previous_credit_hash)
    {
        tree.offset = offset;
    }

    CreditBatch::CreditBatch(const CreditBatch& other)
    {
        previous_credit_hash = other.previous_credit_hash;
        tree.offset = other.tree.offset;
        foreach_(Credit credit, other.credits)
            Add(credit);
    }

    string_t CreditBatch::ToString() const
    {
        uint160 root = CreditBatch(*this).Root();
        stringstream ss;
        ss << "\n============== Credit Batch =============" << "\n"
           << "== Previous Credit Hash: " 
           << previous_credit_hash.ToString() << "\n"
           << "== Offset: " << tree.offset << "\n"
           << "==" << "\n"
           << "== Credits:" << "\n";
        foreach_(Credit credit, credits)
        {
            ss << "== " << ByteString(credit.keydata) << " " 
               << credit.amount << "\n";
        }
        ss << "== " << "\n"
           << "== Root: " << root.ToString() << "\n"
           << "============ End Credit Batch ===========" << "\n";
        return ss.str();
    }

    bool CreditBatch::Add(Credit credit)
    {
        credits.push_back(credit);
        vch_t serialized_credit = credit.getvch();
        if (VectorContainsEntry(serialized_credits, serialized_credit))
        {
            log_ << "CreditBatch::Add: serialized_credit already in batch!\n";
            return false;
        }
        serialized_credits.push_back(serialized_credit);
        tree.AddElement(serialized_credits.back());
        log_ << "CreditBatch(): added credit with amount: " 
             << credit.amount << "\n";
        return true;
    }

    bool CreditBatch::AddCredits(std::vector<Credit> credits_)
    {
        foreach_(const Credit& credit, credits_)
        {
            vch_t serialized_credit = credit.getvch();
            if (VectorContainsEntry(serialized_credits, serialized_credit))
                return false;
        }
        foreach_(const Credit& credit, credits_)
            Add(credit);
        return true;
    }

    uint160 CreditBatch::Root()
    {
        uint160 treeroot = tree.Root();
        uint160 root = SymmetricCombine(previous_credit_hash, treeroot);
        return root;
    }

    int32_t CreditBatch::Position(Credit credit)
    {
        return tree.Position(credit.getvch());
    }

    Credit CreditBatch::GetCredit(uint32_t position)
    {
        return Credit(tree.GetData(position));
    }

    CreditInBatch CreditBatch::GetWithPosition(Credit credit)
    {
        return CreditInBatch(credit, Position(credit), Branch(credit));
    }

    std::vector<uint160> CreditBatch::Branch(Credit credit)
    {
        std::vector<uint160> branch = tree.Branch(Position(credit));
        log_ << "creditbatch::Branch(): tree root is " << tree.Root() << "\n";
        Credit raw_credit(credit.keydata, credit.amount);
        vch_t raw_credit_data = raw_credit.getvch();

        uint160 hash = OrderedDataElement(Position(credit), 
                                          raw_credit_data).Hash();
        
        for (uint32_t i = 0; i < branch.size(); i++)
            hash = SymmetricCombine(hash, branch[i]);

        log_ << "evaluate(hash, branch) = " << hash << "\n";
        branch.push_back(previous_credit_hash);
        branch.push_back(Root());
        return branch;
    }

/*
 *  CreditBatch
 *****************/

/*************************
 *  UnsignedTransaction
 */

    UnsignedTransaction::UnsignedTransaction():
        up_to_date(false), 
        hash(0)
    { }

    string_t UnsignedTransaction::ToString() const
    {
        std::stringstream ss;
        ss << "\n============== UnsignedTransaction =============" << "\n"
           << "== Inputs: \n";
        foreach_(CreditInBatch credit, inputs)
            ss << "== " << credit.ToString() << "\n";
        ss << "==" << "\n"
           << "== Outputs: \n";
        foreach_(Credit credit, outputs)
            ss << "== " << credit.ToString() << "\n";
        ss << "==" << "\n"
           << "== Pubkeys: \n";
        foreach_(Point pubkey, pubkeys)
            ss << "== " << pubkey.ToString() << "\n";
        ss << "==" << "\n"
           << "============ End UnsignedTransaction ===========" << "\n";

        return ss.str();
    }

    void UnsignedTransaction::AddInput(CreditInBatch batch_credit,
                                       bool is_keyhash)
    {
        log_ << "UnsignedTransaction::AddInput: is keyhash: " 
             << is_keyhash << "\n";
        inputs.push_back(batch_credit);
        up_to_date = false;
        if (is_keyhash)
        {
            uint160 keyhash(batch_credit.keydata);
            log_ << "keyhash is " << keyhash << "\n";
            Point pubkey = keydata[keyhash]["pubkey"];
            pubkeys.push_back(pubkey);
            log_ << "added pubkey: " << pubkey << "\n";
        }
    }

    bool UnsignedTransaction::AddOutput(Credit credit)
    {
        if (VectorContainsEntry(outputs, credit))
            return false;
        outputs.push_back(credit);
        up_to_date = false;
        return true;
    }

    uint256 UnsignedTransaction::GetHash()
    {
        if (up_to_date)
            return hash;
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        hash = Hash(ss.begin(), ss.end());
        up_to_date = true;
        return hash;
    }

    bool UnsignedTransaction::operator!()
    {
        return outputs.size() == 0;
    }
/*
 *  UnsignedTransaction
 *************************/


/***********************
 *  SignedTransaction
 */

    string_t SignedTransaction::ToString() const
    {
        std::stringstream ss;
        ss << "\n============== SignedTransaction =============" << "\n"
           << "== Unsigned Transaction: " << rawtx.ToString()
           << "==" << "\n"
           << "== Signature: " << signature.ToString()
           << "== " << "\n"
           << "== Hash160: " << GetHash160().ToString() << "\n"
           << "============ End SignedTransaction ===========" << "\n";

        return ss.str();
    }

    uint256 SignedTransaction::GetHash() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    uint160 SignedTransaction::GetHash160() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }
    
    uint64_t SignedTransaction::IncludedFees()
    {
        uint64_t total_in = 0, total_out = 0;
        foreach_(CreditInBatch& credit, rawtx.inputs)
            total_in += credit.amount;
        
        foreach_(Credit& credit, rawtx.outputs)
            total_out += credit.amount;
        
        if (total_in > total_out)
            return total_in - total_out;

        return 0;
    }

    bool SignedTransaction::Validate()
    {
        uint64_t total_in = 0, total_out = 0;

        set<uint64_t> input_positions;

        if (rawtx.outputs.size() > MAX_OUTPUTS_PER_TRANSACTION)
            return false;

        if (!VerifyTransactionSignature(*this))
        {
            log_ << "ValidateTransaction(): signature verification failed\n";
            return false;
        }

        foreach_(CreditInBatch& credit, rawtx.inputs)
        {
            log_ << "ValidateTransaction(): considering credit with position "
                 << credit.position << "\n";

            if (!CreditInBatchHasValidConnectionToCalendar(credit))
            {
                log_ << "ValidateTransaction: bad credit: " << credit << "\n";
                return false;
            }

            if (input_positions.count(credit.position))
            {
                log_ << "doublespend! position " << credit.position << "\n";
                return false;
            }
            else
            {
                total_in += credit.amount;
                log_ << "adding " << credit.position 
                     << " to spent input positions\n";
                input_positions.insert(credit.position);
            }
        }

        foreach_(const Credit& credit, rawtx.outputs)
        {
            total_out += credit.amount;
        }

        return total_out > MIN_TRANSACTION_SIZE 
                && total_in >= total_out;
    }

    vector<uint160> SignedTransaction::GetMissingCredits()
    {
        vector<uint160> missing_credits;

        foreach_(const CreditInBatch& credit, rawtx.inputs)
        {
            uint160 batch_root = credit.branch.back();   
            uint160 credit_hash = SymmetricCombine(batch_root,
                                                   credit.diurn_branch[0]);
            if (credit.diurn_branch.size() == 1)
            {
                if (!creditdata[credit_hash].HasProperty("mined_credit") &&
                    !creditdata[credit_hash].HasProperty("branch"))
                {
                    missing_credits.push_back(credit_hash);
                }
            }
        }
        return missing_credits;
    }

/*
 *  SignedTransaction
 ***********************/
