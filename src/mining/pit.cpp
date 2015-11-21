// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#include "mining/pit.h"
#include "flexnode/batchchainer.h"
#include "flexnode/flexnode.h"
#include "relays/relaystate.h"

#include "log.h"
#define LOG_CATEGORY "pit.cpp"

extern CMemoryDB batchhashesdb;

/**************
 *  MemPool
 */

    void MemPool::CountDoubleSpend(SignedTransaction tx)
    {
        std::set<uint160>::iterator it;
        for (it = transactions.begin(); it != transactions.end(); ++it)
        {
            SignedTransaction tx_ = creditdata[*it]["tx"];
            if (TransactionsConflict(tx_, tx))
            {
                multiple_spends[tx.GetHash160()].insert(tx_.GetHash160());
                multiple_spends[tx_.GetHash160()].insert(tx.GetHash160());
            }
        }
    }

    bool MemPool::CheckForDroppedTransactions(
        uint160 credit_hash,
        std::vector<uint160> missing_txs,
        std::vector<uint160> extra_txs, 
        std::vector<uint160>& dropped_txs)
    {
        foreach_(const uint160& txhash, missing_txs)
        {
            uint64_t depth = TransactionDepth(txhash, credit_hash);
            if (depth > 2)
            {
                if (!multiple_spends.count(txhash))
                {
                    dropped_txs.push_back(txhash);
                    continue;
                }

                std::set<uint160> conflicts = multiple_spends[txhash];
                bool new_chain_contains_double_spend = false;
                
                foreach_(const uint160& other_txhash, conflicts)
                {
                    if (VectorContainsEntry(extra_txs, other_txhash))
                    {
                        new_chain_contains_double_spend = true;
                        break;
                    }
                }

                if (!new_chain_contains_double_spend)
                    dropped_txs.push_back(txhash);
            }
        }

        return dropped_txs.size() > 0;
    }

    bool MemPool::ChangeChain(uint160 from_hash, uint160 to_hash, 
                              std::vector<uint160>& dropped_txs)
    {
        std::vector<uint160> extra_txs, missing_txs;
        
        if (!GetTxDifferences(from_hash, to_hash, extra_txs, missing_txs))
            return false;
        
        if (CheckForDroppedTransactions(from_hash, missing_txs, 
                                        extra_txs, dropped_txs))
        {
            log_ << "MemPool::ChangeChain:: dropped transactions!\n";
            return false;
        }
        
       
        foreach_(const uint160& txhash, extra_txs)
        {
            if (transactions.count(txhash))
                transactions.erase(txhash);
        }

        foreach_(const uint160& txhash, missing_txs)
        {
            if (!transactions.count(txhash))
                transactions.insert(txhash);
        }
        return true;
    }

    void MemPool::AddTransaction(SignedTransaction tx)
    {
        if (transactions.count(tx.GetHash160()))
            return;
        CountDoubleSpend(tx);
        transactions.insert(tx.GetHash160());
    }

    void MemPool::AddMinedCredit(MinedCredit mined_credit)
    {
        mined_credits.insert(mined_credit.GetHash160());
    }

    bool MemPool::ContainsTransaction(uint160 txhash)
    {
        return transactions.count(txhash);
    }

    bool MemPool::ContainsMinedCredit(uint160 mined_credit_hash)
    {
        return mined_credits.count(mined_credit_hash);
    }

    bool MemPool::Contains(uint160 hash)
    {
        return ContainsTransaction(hash) || ContainsMinedCredit(hash);
    }

    bool MemPool::IsDoubleSpend(uint160 hash)
    {
       return multiple_spends[hash].size() > 0;
    }

    void MemPool::RemoveTransaction(uint160 txhash)
    {
        transactions.erase(txhash);
        std::set<uint160> conflicts = multiple_spends[txhash];
        std::set<uint160>::iterator it;
        for (it = conflicts.begin(); it != conflicts.end(); ++it)
            multiple_spends[*it].erase(txhash);
        multiple_spends.erase(txhash);
    }

    void MemPool::RemoveMinedCredit(uint160 mined_credit_hash)
    {
        mined_credits.erase(mined_credit_hash);
    }

    void MemPool::Remove(uint160 hash)
    {
        if (ContainsTransaction(hash))
            RemoveTransaction(hash);
        else if (ContainsMinedCredit(hash))
            RemoveMinedCredit(hash);
    }

    void MemPool::RemoveMany(std::vector<uint160> hashes)
    {
        for (uint32_t i = 0; i < hashes.size(); i++)
            Remove(hashes[i]);
    }

/*
 *  MemPool
 *************/


/***************
 *  CreditPit
 */

    CreditPit::CreditPit() { }

    void CreditPit::Initialize()
    {
        started = false;
        flexnode.digging = false;
        hashrate = 0;
        Point pubkey = flexnode.wallet.NewKey();
        mined_credit.keydata = pubkey.getvch();
        mined_credit.amount = CREDIT_NUGGET;
    }

    void CreditPit::ResetBatch()
    {
        log_ << "CreditPit::ResetBatch()\n";
        uint160 credit_hash = flexnode.previous_mined_credit_hash;
        
        MinedCredit credit = creditdata[credit_hash]["mined_credit"];

        MinedCreditMessage prev_msg
            = creditdata[flexnode.previous_mined_credit_hash]["msg"];
        CreditBatch batch_ = ReconstructBatch(prev_msg);
        uint64_t num_credits = batch_.credits.size();
        
        batch = CreditBatch(credit_hash, credit.offset + num_credits);
        
        in_current_batch.erase(in_current_batch.begin(), 
                               in_current_batch.end());

        log_ << batch;
    }

    void CreditPit::MarkAsEncoded(uint160 hash)
    {
        encoded_in_chain.insert(hash);
    }

    void CreditPit::MarkAsUnencoded(uint160 hash)
    {
        encoded_in_chain.erase(hash);
    }

    bool CreditPit::IsAlreadyEncoded(uint160 hash)
    {
        return encoded_in_chain.count(hash);
    }

    void CreditPit::RemoveFromAccepted(std::vector<uint160> hashes)
    {
        foreach_(const uint160& hash, hashes)
        {
            accepted_mined_credits.erase(hash);
            accepted_txs.erase(hash);
            EraseEntryFromVector(hash, accepted_relay_msgs);
        }
    }

    void CreditPit::AddTransactionToCurrentBatch(SignedTransaction tx)
    {
        uint160 txhash = tx.GetHash160();
        accepted_txs.insert(txhash);
        
        if (!batch.AddCredits(tx.rawtx.outputs))
        {
            log_ << "batch contains conflicting credits:"
                 << batch << " contains credits conflicting with "
                 << tx.rawtx.outputs << " - leaving in queue\n";
            return;
        }
        
        in_current_batch.insert(txhash);
    }

    void CreditPit::HandleNugget(MinedCredit mined_credit)
    {
        uint160 nugget_hash = mined_credit.GetHash160();
        if (in_current_batch.count(nugget_hash))
        {
            log_ << "already included this nugget in current batch\n";
            return;
        }
        if (encoded_in_chain.count(nugget_hash))
        {
            log_ << "this nugget already encoded in chain\n";
            return;
        }
        pool.AddMinedCredit(mined_credit);
        accepted_mined_credits.insert(nugget_hash);
        pool.already_included.insert(nugget_hash);
        log_ << "HandleNugget:: adding to batch\n";
        batch.Add(Credit(mined_credit.keydata, mined_credit.amount));
        log_ << "HandleNugget(): batch.credits.size is now "
             << batch.credits.size() << "\n";
        in_current_batch.insert(nugget_hash);
    }

    void CreditPit::PrepareMinedCredit()
    {
        log_ << "PrepareMinedCredit()\n";

        uint160 previous_credit_hash = flexnode.previous_mined_credit_hash;

        mined_credit.previous_diurn_root
            = flexnode.calendar.PreviousDiurnRoot();

        mined_credit.diurnal_block_root
            = flexnode.calendar.current_diurn.diurnal_block.Root();

        mined_credit.diurnal_difficulty
            = flexnode.calendar.current_diurn.current_difficulty;   

        Point pubkey = flexnode.wallet.NewKey();
        mined_credit.keydata = pubkey.getvch();

        mined_credit.previous_mined_credit_hash = previous_credit_hash;

        MinedCredit previous_mined_credit
            = creditdata[previous_credit_hash]["mined_credit"];

        log_ << "previous credit was: " << previous_mined_credit;

        mined_credit.batch_number = previous_mined_credit.batch_number + 1;
        mined_credit.total_work = previous_mined_credit.total_work
                                  + previous_mined_credit.difficulty;

        RelayState state = GetRelayState(previous_credit_hash);
        state.latest_credit_hash = previous_credit_hash;

        RelayStateChain relay_chain(state);

        MinedCreditMessage prev_msg = creditdata[previous_credit_hash]["msg"];
        prev_msg.hash_list.RecoverFullHashes();
        RemoveFromAccepted(prev_msg.hash_list.full_hashes);

        for (uint32_t i = 0; i < 10; i++)
        {
            prev_msg = creditdata[prev_msg.mined_credit.previous_mined_credit_hash]["msg"];
            prev_msg.hash_list.RecoverFullHashes();
            RemoveFromAccepted(prev_msg.hash_list.full_hashes);
            if (prev_msg.mined_credit.previous_mined_credit_hash == 0)
                break;
        }

        vector<uint160> orphan_batches;
        foreach_(const uint160& credit_hash, accepted_mined_credits)
        {
            MinedCredit credit = creditdata[credit_hash]["mined_credit"];
            if (credit.batch_number <= previous_mined_credit.batch_number
                    && !InMainChain(credit_hash))
            {
                log_ << "removing orphan batch " << credit_hash << "\n";
                orphan_batches.push_back(credit_hash);
            }
        }
        RemoveFromAccepted(orphan_batches);


        vector<uint160> rejected;
        relay_chain.HandleMessages(accepted_relay_msgs, rejected);
        if (rejected.size() > 0)
        {
            log_ << "Removing rejected messages: " << rejected << "\n";
            RemoveFromAccepted(rejected);
        }

        if (previous_mined_credit.batch_number > 0 &&
            !accepted_mined_credits.count(previous_credit_hash))
            accepted_mined_credits.insert(previous_credit_hash);

        ResetBatch();
        BitChain spent_chain = GetSpentChain(previous_credit_hash);

        log_ << "PrepareMinedCredit(): before adding accepted: "
             << "spent chain length is " << spent_chain.length << "\n"
             << "hash is " << spent_chain.GetHash160() << "\n";
        AddAccepted();

        log_ << "PrepareMinedCredit(): initial batch " << batch;
        
        mined_credit.offset = batch.tree.offset;
        log_ << "PrepareMinedCredit: This batch has "
             << batch.credits.size() << " credits\n";

        SetMinedCreditContents();

        mined_credit.relay_state_hash = relay_chain.state.GetHash160();


        log_ << "num valid relays = "
             << relay_chain.state.NumberOfValidRelays() << "\n";
        
        mined_credit.timestamp = GetTimeMicros();
        mined_credit.difficulty = GetNextDifficulty(previous_mined_credit);

        uint160 prev_hash = mined_credit.previous_mined_credit_hash;
        if (calendardata[prev_hash]["is_calend"])
        {
            log_ << "previous mined credit was calend - adding fees\n";
            AddFees();
        }
        else
        {
            log_ << prev_hash << " not a calend - no fees\n";
        }

        spent_chain.SetLength(spent_chain.length + batch.credits.size());
        log_ << batch.credits.size() << " credits in batch" << "\n";

        foreach_(uint160 txhash, accepted_txs)
        {
            SignedTransaction tx = creditdata[txhash]["tx"];
            foreach_(CreditInBatch credit, tx.rawtx.inputs)
                spent_chain.Set(credit.position);
        }

        log_ << "PrepareMinedCredit(): after adding accepted: "
             << "spent chain length is " << spent_chain.length << "\n"
             << "hash is " << spent_chain.GetHash160() << "\n";

        mined_credit.spent_chain_hash = spent_chain.GetHash160();

        mined_credit.batch_root = batch.Root();
        RecordRelayState(relay_chain.state, mined_credit.GetHash160());

        SetMinedCreditHashListHash();

        stringstream ss;
        ss << "\n============== Prepared for Mining =============" << "\n"
           << "== Batch Number: " << mined_credit.batch_number << "\n"
           << "== Credit Hash: " << mined_credit.GetHash160().ToString() << "\n"
           << "==" << "\n"
           << "== Hashes Included: " << "\n";
        foreach_(uint160 hash, mined_credit_contents)
        {
            string_t type = msgdata[hash]["type"];
            ss << "== " << hash.ToString() << "  " << type << "\n";
        }
        ss << "==" << "\n"
           << "== Timestamp: " << mined_credit.timestamp << "\n"
           << "============ End Prepared for Mining ===========" << "\n";
        log_ << ss.str();
    }

    void CreditPit::SetMinedCreditHashListHash()
    {
        ShortHashList<uint160> hash_list;

        foreach_(const uint160& hash, mined_credit_contents)
            hash_list.full_hashes.push_back(hash);

        hash_list.GenerateShortHashes();

        mined_credit.hash_list_hash = hash_list.GetHash160();
    }

    void CreditPit::SetMinedCreditContents()
    {
        mined_credit_contents.resize(0);

        mined_credit_contents.insert(mined_credit_contents.end(),
                                     in_current_batch.begin(),
                                     in_current_batch.end());


        mined_credit_contents.insert(mined_credit_contents.end(),
                                     accepted_relay_msgs.begin(),
                                     accepted_relay_msgs.end());
    }

    uint256 CreditPit::SeedMemory()
    {   
        uint256 seed = mined_credit.GetHash();
        return seed;
    }

    void CreditPit::HandleTransaction(SignedTransaction tx)
    {
        log_ << "CreditPit: HandleTransaction()\n";
        uint160 txhash = tx.GetHash160();
        if (in_current_batch.count(txhash))
        {
            log_ << "transaction already included in current batch\n";
            return;
        }
        if (encoded_in_chain.count(txhash))
        {
            log_ << "transaction already encoded in chain\n";
            return;
        }
        
        pool.AddTransaction(tx);
        
        if (!pool.IsDoubleSpend(txhash))
            AddTransactionToCurrentBatch(tx);
    }

    void CreditPit::HandleRelayMessage(uint160 msg_hash)
    {
        log_ << "CreditPit::HandleRelayMessage(): " << msg_hash << "\n";
        if (!VectorContainsEntry(accepted_relay_msgs, msg_hash))
            accepted_relay_msgs.push_back(msg_hash);
    }

    void CreditPit::AddBatchToTip(MinedCreditMessage& msg)
    {
        log_ << "CreditPit::AddBatchToTip(): " << msg.mined_credit << "\n";
        vector<uint160> hashes_to_delete, remaining_relay_msgs;

        msg.hash_list.RecoverFullHashes();
        
        uint32_t new_batch_number = msg.mined_credit.batch_number;

        foreach_(const uint160& hash, msg.hash_list.full_hashes)
        {
            if (creditdata[hash].HasProperty("tx"))
            {
                log_ << "New tip contains tx " << hash << "\n";
                SignedTransaction tx = creditdata[hash]["tx"];
                hashes_to_delete.push_back(hash);
                MarkAsEncoded(hash);
            }
        }
        foreach_(const uint160& hash, accepted_txs)
        {
            log_ << "transaction: " << hash << "\n";
            SignedTransaction tx = creditdata[hash]["tx"];
            if (VectorContainsEntry(msg.hash_list.full_hashes, hash))
            {
                hashes_to_delete.push_back(hash);
                MarkAsEncoded(hash);
            }
        }
        foreach_(const uint160& hash, accepted_mined_credits)
        {
            log_  << "mined credit: " << hash << "\n";
            MinedCredit mined_credit = creditdata[hash]["mined_credit"];
            if (VectorContainsEntry(msg.hash_list.full_hashes, hash)
                || mined_credit.batch_number <= new_batch_number)
            {
                log_ << "removing " << hash << "from accepted\n";
                hashes_to_delete.push_back(hash);
                MarkAsEncoded(hash);
            }
            else
            {
                uint160 credit_hash = msg.mined_credit.GetHash160();
                log_ << credit_hash << " does not contain " << hash << "\n";
            }
        }
        foreach_(const uint160& hash, accepted_relay_msgs)
        {
            if (VectorContainsEntry(msg.hash_list.full_hashes, hash))
            {
                log_ << "CreditPit::AddBatchToTip removing accepted "
                     << "relay msg: " << hash << "\n"
                     << "because it's in the chain\n";
                hashes_to_delete.push_back(hash);
                MarkAsEncoded(hash);
            }
            else
                remaining_relay_msgs.push_back(hash);
        }

        RemoveFromAccepted(hashes_to_delete);

        ResetBatch();

        for (uint32_t i = 0; i < remaining_relay_msgs.size(); i++)
        {
            uint160 msg_hash = remaining_relay_msgs[i];
            HandleRelayMessage(msg_hash);
        }
    }

    uint160 CreditPit::LinkThreshold()
    {
        return Target() * LINKS_PER_PROOF;
    }

    uint160 CreditPit::Target()
    {
        uint160 difficulty = mined_credit.difficulty;
        if (difficulty == 0)
            difficulty = INITIAL_DIFFICULTY;
        difficulty = difficulty / NUGGETS_PER_BATCH;

        uint160 numerator = 1;
        numerator <<= 128;
        return numerator / difficulty;
    }

    bool CreditPit::SwitchToNewChain(MinedCreditMessage msg, CNode* pfrom)
    {
        std::vector<uint160> dropped_txs;
        if (!pool.ChangeChain(flexnode.previous_mined_credit_hash, 
                              msg.mined_credit.GetHash160(),
                              dropped_txs))
        {
            log_ << "pool failed to change chain\n";
            if (pfrom)
            {
                foreach_(const uint160& txhash, dropped_txs)
                {
                    SignedTransaction tx = creditdata[txhash]["tx"];
                    TellNodeAboutTransaction(pfrom, tx);
                }
            }
            return false;
        }
        return true;
    }

    void CreditPit::AddFees()
    {
        uint160 previous_credit_hash = mined_credit.previous_mined_credit_hash;
        RelayState state = GetRelayState(previous_credit_hash);

        vector<Credit> fees_owed = GetFeesOwed(state);

        foreach_(Credit fee, fees_owed)
        {
            batch.Add(fee);
            log_ << fee.keydata[0] << fee.keydata[1] 
                 << " " << fee.amount << "\n";
        }
    }

    void CreditPit::SwitchToNewCalend(uint160 calend_credit_hash)
    {
        bool was_digging = flexnode.digging;
        flexnode.digging = false;

        log_ << "SwitchToNewCalend() - " << calend_credit_hash << "\n";

        ResetBatch();
        
        vector<uint160> txs_to_delete;
        vector<uint160> mined_credits_to_delete;

        MinedCredit mined_credit
            = creditdata[calend_credit_hash]["mined_credit"];

        uint160 diurn_root = SymmetricCombine(mined_credit.previous_diurn_root,
                                              mined_credit.diurnal_block_root);
        log_ << "diurn root is " << diurn_root << "\n"
             << "calend credit " << mined_credit << "\n";

        flexnode.previous_mined_credit_hash = mined_credit.GetHash160();

        log_ << "hash is " << flexnode.previous_mined_credit_hash << "\n";

        foreach_(const uint160& txhash, accepted_txs)
        {
            log_ << "considering accepted tx " << txhash << "\n";
            SignedTransaction tx = creditdata[txhash]["tx"];
            if (TxContainsSpentInputs(tx))
            {
                log_ << "contains spent inputs\n";
                txs_to_delete.push_back(txhash);
            }
        }

        foreach_(const uint160& mined_credit_hash, accepted_mined_credits)
        {
            log_ << "considering mined credit hash " 
                 << mined_credit_hash << "\n";
            MinedCredit credit = creditdata[mined_credit_hash]["mined_credit"];
            log_ << "mined credit is " << credit;
            if (credit.previous_diurn_root != diurn_root)
            {
                log_ << "credit has wrong previous diurn root: "
                     << credit.previous_diurn_root << "\n";
                mined_credits_to_delete.push_back(mined_credit_hash);
            }
        }
        
        foreach_(const uint160& txhash, txs_to_delete)
        {
            accepted_txs.erase(txhash);
            pool.RemoveTransaction(txhash);
        }
        
        foreach_(const uint160& mined_credit_hash, mined_credits_to_delete)
        {
            accepted_mined_credits.erase(mined_credit_hash);
            pool.RemoveMinedCredit(mined_credit_hash);
        }
        
        PrepareMinedCredit();

        uint160 credit_hash = mined_credit.GetHash160();

        RelayState state = GetRelayState(credit_hash);

        log_ << "generating new relay handler\n";
        log_ << "Relay State for " << credit_hash << " is " << state << "\n";

        state.latest_credit_hash = credit_hash;
        flexnode.relayhandler = RelayHandler(state);

        log_ << "adding accepted\n";
        AddAccepted();

        flexnode.digging = was_digging;
    }

    void CreditPit::AddAccepted()
    {
        foreach_(const uint160& txhash, accepted_txs)
        {
            SignedTransaction tx = creditdata[txhash]["tx"];
            HandleTransaction(tx);
        }
        foreach_(const uint160& nugget_hash, accepted_mined_credits)
        {
            MinedCreditMessage msg = creditdata[nugget_hash]["msg"];
            HandleNugget(msg.mined_credit);
        }
        foreach_(const uint160& msg_hash, accepted_relay_msgs)
            HandleRelayMessage(msg_hash);
    }

    void CreditPit::StartMining()
    {
        if (!started)
        {
            boost::thread t(&CreditPit::MineForCredits, this);
            started = true;
        }
        else
        {
            flexnode.digging = true;
            flexnode.downloader.finished_downloading = true;
        }
    }

    void CreditPit::HandleNewProof()
    {
        log_ << "HandleNewProof()\n";
        MinedCreditMessage msg;
        msg.proof = new_proof;
        msg.mined_credit = mined_credit;

        msg.hash_list = ShortHashList<uint160>();

        foreach_(const uint160& hash, mined_credit_contents)
        {
            msg.hash_list.full_hashes.push_back(hash);
        }
        uint160 credit_hash = mined_credit.GetHash160();
        creditdata[credit_hash]["mined_credit"] = mined_credit;
        msg.hash_list.GenerateShortHashes();

        log_ << "sending " << credit_hash << " to flexnode\n";
        flexnode.HandleMinedCreditMessage(msg, NULL);
        
        if (InMainChain(credit_hash))
        {
            log_ << "adding relay duty\n";
            AddRelayDutyForBatch(mined_credit.GetHash160());
        }
    }

    void CreditPit::MineForCredits()
    {
        flexnode.downloader.finished_downloading = true;
        flexnode.digging = true;
        SeedMemory();
        
        while (true)
        {
            try
            {
                if (flexnode.digging)
                {
                    PrepareMinedCredit();
                    log_ << "working on the mined credit:" << mined_credit;
                    TwistWorkProof proof(SeedMemory(), BLOCK_MEMORY_FACTOR, 
                                         Target(), LinkThreshold(), 
                                         SEGMENTS_PER_PROOF);

                    uint64_t start_time = GetTimeMicros();
                    bool result = proof.DoWork(&flexnode.digging);
                    log_ << "proof.length is " << proof.Length() << "\n";
                    hashrate = 0.5 * (hashrate + 
                                        1000000 * proof.Length() / 
                                                (GetTimeMicros() - start_time));

                    if (result && proof.num_links >= proof.num_segments / 4)
                    {
                        new_proof = proof;
                        {
                            LOCK(flexnode.mutex);
                            log_ << "mining successful. handling new proof\n";
                            HandleNewProof();
                        }
                        PrepareMinedCredit();
                        SeedMemory();
                    }
                    else
                    {
                        log_ << "bad proof\n";
                    }
                }
                boost::this_thread::sleep(boost::posix_time::milliseconds(103));
            }
            catch(boost::thread_interrupted&)
            {
                fprintf(stderr, "Thread is stopped\n");
                return;
            }
        }
    }

    void CreditPit::StopMining()
    {
        flexnode.digging = false;
        ResetBatch();
        accepted_txs = std::set<uint160>();
        accepted_mined_credits = std::set<uint160>();
        accepted_relay_msgs = std::vector<uint160>();
        hashrate = 0;
    }

/*
 *  CreditPit
 **************/

 
