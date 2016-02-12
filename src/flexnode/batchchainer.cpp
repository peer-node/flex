// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#include "flexnode/flexnode.h"

#include "log.h"
#define LOG_CATEGORY "batchchainer.cpp"

using namespace std;

vector<Credit> GetAllTransactionOutputs(MinedCreditMessage& msg)
{
    vector<Credit> outputs;
    msg.hash_list.RecoverFullHashes();

    foreach_(const uint160& hash, msg.hash_list.full_hashes)
    {
        if (creditdata[hash].HasProperty("tx"))
        {
            SignedTransaction tx = creditdata[hash]["tx"];
            outputs.insert(outputs.end(), 
                           tx.rawtx.outputs.begin(),
                           tx.rawtx.outputs.end());
        }
    }
    return outputs;
}

vector<Credit> GetAllMinedCredits(MinedCreditMessage& msg)
{
    vector<Credit> credits;
    msg.hash_list.RecoverFullHashes();

    foreach_(const uint160& hash, msg.hash_list.full_hashes)
    {
        if (creditdata[hash].HasProperty("mined_credit"))
        {
            MinedCredit mined_credit = creditdata[hash]["mined_credit"];
            credits.push_back((Credit)mined_credit);
        }
    }
    return credits;
}


CreditBatch ReconstructBatch_(MinedCreditMessage& msg)
{
    CreditBatch credit_batch(msg.mined_credit.previous_mined_credit_hash,
                             msg.mined_credit.offset);
    vector<Credit> credits 
        = GetCreditsFromHashes(msg.hash_list.full_hashes);

    uint160 prev_hash = msg.mined_credit.previous_mined_credit_hash;

    if (calendardata[prev_hash]["is_calend"])
    {
        RelayState relay_state = GetRelayState(prev_hash);
        vector<Credit> fees = GetFeesOwed(relay_state);
        credits.insert(credits.end(), fees.begin(), fees.end());
    }
    for (uint32_t i = 0; i < credits.size(); i++)
        credit_batch.Add(credits[i]);
    return credit_batch;
}

/****************
 * BatchChainer
 */

    void BatchChainer::SwitchToBestChain()
    {
        uint160 credit_hash = MostWorkBatch();
        bool was_digging = flexnode.digging;
        
        flexnode.digging = false;

        if (HaveSameCalend(flexnode.previous_mined_credit_hash, 
                           credit_hash))
            SwitchToChainViaFork(credit_hash);
        else
            SwitchToChainViaCalend(credit_hash);

        flexnode.calendar = Calendar(credit_hash);

        flexnode.digging = was_digging;
    }

    void BatchChainer::RewindToCalend(uint160 calend_credit_hash)
    {
        while (flexnode.previous_mined_credit_hash != calend_credit_hash)
            RemoveBatchFromTip();
    }

    void BatchChainer::SwitchToChainViaCalend(uint160 credit_hash)
    {
        log_ << "SwitchToChainViaCalend: " << credit_hash << "\n";
        uint160 calend_hash = GetCalendCreditHash(credit_hash);
        log_ << "calend credit hash is " << calend_hash << "\n";

        if (InMainChain(calend_hash))
        {
            log_ << "in main chain - rewinding to calend\n";
            RewindToCalend(calend_hash);
        }
        else
        {
            log_ << "not in main chain - initiating at calend\n";
            flexnode.InitiateChainAtCalend(calend_hash);
            total_work = creditdata[calend_hash].Location("total_work");
        }

        vector<uint160> credit_hashes
            = GetCreditHashesSinceCalend(calend_hash, credit_hash);

        log_ << "credit hashes since calend:\n";
        foreach_(uint160 hash, credit_hashes)
            log_ << hash << "\n";

        foreach_(const uint160& credit_hash_, credit_hashes)
        {
            MinedCreditMessage msg = creditdata[credit_hash_]["msg"];
            log_ << "adding " << credit_hash_ << " to tip\n";
            AddBatchToTip(msg);
        }
        log_ << "SwitchToChainViaCalend(): complete" << "\n"
             << "calendar is now " <<  flexnode.calendar << "\n"
             << "credit hash is " << credit_hash << "\n";

        Calendar calendar = calendardata[credit_hash]["calendar"];
        log_ << "calendar of " << credit_hash << " is " << calendar << "\n";
    }

    void BatchChainer::SwitchToChainViaFork(uint160 credit_hash)
    {
        uint32_t stepsback;

        uint160 fork = GetFork(flexnode.previous_mined_credit_hash,
                               credit_hash, 
                               stepsback);

        while (flexnode.previous_mined_credit_hash != fork)
        {
            log_ << "SwitchToChainViaFork(): removing "
                 << flexnode.previous_mined_credit_hash
                 << "\n";
            flexnode.RemoveBatchFromTip();
        }

        deque<MinedCreditMessage> mined_credit_messages;
        while (credit_hash != fork)
        {
            MinedCreditMessage msg = creditdata[credit_hash]["msg"];
            mined_credit_messages.push_front(creditdata[credit_hash]["msg"]);
            credit_hash = msg.mined_credit.previous_mined_credit_hash;
        }

        foreach_(MinedCreditMessage& msg, mined_credit_messages)
        {
            log_ << "SwitchToChainViaFork(): adding "
                 << msg.mined_credit.GetHash160() << "\n";
            flexnode.AddBatchToTip(msg);
        }
    }

    void BatchChainer::CheckForPaymentsToWatchedPubKeys(SignedTransaction tx,
                                                        uint160 credit_hash)
    {
        log_ << "CheckForPaymentsToWatchedPubKeys(): " << tx;

        foreach_(const Credit& credit, tx.rawtx.outputs)
        {
            log_ << "considering credit " << credit << "\n";
            if (credit.keydata.size() != 34)
            {
                log_ << "no pubkey specified\n";
                continue;
            }
            Point pubkey;
            pubkey.setvch(credit.keydata);
            log_ << "CheckForPaymentsToWatchedPubKeys: checking "
                 << pubkey << "\n";
            if (tradedata[pubkey].HasProperty("accept_commit_hash"))
            {
                log_ << "yes - watched\n";
                uint160 accept_commit_hash
                    = tradedata[pubkey]["accept_commit_hash"];
                
                uint64_t amount_received
                    = tradedata[accept_commit_hash]["received"];
                
                amount_received += credit.amount;
                tradedata[accept_commit_hash]["received"] = amount_received;
                tradedata[accept_commit_hash]["credit_hash"] = credit_hash;
                
                log_ << "accept commit hash " << accept_commit_hash
                     <<" has received " << amount_received << "\n";
            }
            else
            {
                log_ << "no.\n";
            }
        }
    }

    void BatchChainer::AddBatchToTip(MinedCreditMessage& msg)
    {
        uint160 credit_hash = msg.mined_credit.GetHash160();
        log_ << "BatchChainer::AddBatchToTip(): " << credit_hash << "\n";

        msgdata[credit_hash]["received"] = true;

        vector<SignedTransaction> remaining_txs;
        vector<MinedCredit> remaining_mined_credits;

        msg.hash_list.RecoverFullHashes();

        foreach_(const uint160& hash, msg.hash_list.full_hashes)
        {
            log_ << "checking " << hash << " for transactions\n";
            string_t type = msgdata[hash]["type"];
            if (type == "tx")
            {
                log_ << hash << " is a transaction\n";
                SignedTransaction tx = creditdata[hash]["tx"];
                UpdateSpentChain(tx.rawtx);
                flexnode.wallet.HandleTransaction(tx);
                log_ << "checking for payments to watched pubkeys\n";
                CheckForPaymentsToWatchedPubKeys(tx, credit_hash);
            }
        }
        foreach_(const uint160& hash, flexnode.pit.accepted_txs)
        {
            SignedTransaction tx = creditdata[hash]["tx"];
            if (!VectorContainsEntry(msg.hash_list.full_hashes, hash))
                remaining_txs.push_back(tx);
        }
        foreach_(const uint160& hash, flexnode.pit.accepted_mined_credits)
        {
            MinedCredit mined_credit = creditdata[hash]["mined_credit"];
            if (VectorContainsEntry(msg.hash_list.full_hashes, hash))
                flexnode.spent_chain.Add();
            else
                remaining_mined_credits.push_back(mined_credit);
        }

        foreach_(SignedTransaction tx, remaining_txs)
            HandleTransaction(tx, NULL);
        foreach_(MinedCredit credit, remaining_mined_credits)
            HandleNugget(credit);

        total_work = msg.mined_credit.total_work + msg.mined_credit.difficulty;
    }

    void BatchChainer::RemoveBatchFromTip()
    {
        MinedCreditMessage msg
            = creditdata[flexnode.previous_mined_credit_hash]["msg"];
        uint160 preceding_hash = msg.mined_credit.previous_mined_credit_hash;

        msg.hash_list.RecoverFullHashes();
        CreditBatch prev_batch = ReconstructBatch_(msg);
        
        flexnode.wallet.UnhandleBatch(prev_batch);

        foreach_(const uint160& hash, msg.hash_list.full_hashes)
        {
            string_t type = msgdata[hash]["type"];
            if (type == "tx")
            {
                SignedTransaction tx = creditdata[hash]["tx"];
                flexnode.wallet.UnhandleTransaction(tx);
                flexnode.pit.MarkAsUnencoded(hash);
                HandleTransaction(tx, NULL);
            }
            if (type == "mined_credit")
            {
                MinedCredit credit = creditdata[hash]["mined_credit"];
                flexnode.pit.MarkAsUnencoded(hash);
                HandleNugget(credit);
            }
        }
        flexnode.previous_mined_credit_hash = preceding_hash;
    }

    void BatchChainer::HandleBatch(MinedCreditMessage msg, CNode* peer)
    {
        log_ << "HandleBatch(): " << msg;

        if (flexnode.pit.IsAlreadyEncoded(msg.GetHash160()))
        {
            log_ << "batch already encoded in chain\n";
            return;
        }
        if (!msg.hash_list.RecoverFullHashes())
        {
            log_ << "Couldn't recover full hashes\n";
            return;
        }
        if (!flexnode.message_validator.ValidateMinedCreditMessage(msg))
        {
            log_ << "couldn't validate batch\n";
            return;
        }
        
        RecordBatch(msg);

        uint160 work = RecordTotalWork(msg);

        if (work > total_work)
        {
            log_ << "HandleBatch(): new maximum work"
                 << work  << " > " << total_work << "\n";
            HandleNewMostWorkBatch(msg);
        }

        HandleNonOrphansAfterBatch(msg);
    }

    uint160 BatchChainer::RecordTotalWork(MinedCreditMessage msg)
    {
        MinedCredit mined_credit = msg.mined_credit;
        uint160 work = mined_credit.total_work + mined_credit.difficulty;

        uint160 credit_hash = mined_credit.GetHash160();
        creditdata[credit_hash].Location("total_work") = work;
        return work;
    }

    void BatchChainer::HandleNonOrphansAfterBatch(MinedCreditMessage msg)
    {
        vector<SignedTransaction> non_orphan_txs;
        non_orphan_txs = orphanage.TxNonOrphansAfterBatch(
                                        msg.mined_credit.batch_root);
        foreach_(const SignedTransaction& tx, non_orphan_txs)
            HandleTransaction(tx, NULL);

        vector<MinedCreditMessage> non_orphan_nuggets;
        uint160 credit_hash = msg.mined_credit.GetHash160();
        non_orphan_nuggets = orphanage.NuggetNonOrphansAfterBatch(credit_hash);
        foreach_(const MinedCreditMessage& msg_, non_orphan_nuggets)
            HandleMinedCreditMessage(msg_, NULL);
    }

    void BatchChainer::HandleNewMostWorkBatch(MinedCreditMessage msg)
    {
        log_ << "HandleNewMostWorkBatch(): " << msg;
        
        bool was_digging = flexnode.digging;
        flexnode.digging = 0;

        uint160 credit_hash = msg.mined_credit.GetHash160();
        log_ << "switching to new chain\n";
        if (!flexnode.pit.SwitchToNewChain(msg, NULL))
            return;
        log_ << "switched to new chain\n";
        vector<MinedCreditMessage> non_orphan_nuggets;
    
        msg.hash_list.RecoverFullHashes();

        flexnode.pit.pool.RemoveMany(msg.hash_list.full_hashes);
        StoreMinedCreditMessage(msg);
        SwitchToChainViaFork(credit_hash);
        flexnode.wallet.HandleNewBatch(ReconstructBatch_(msg));
        if (msg.mined_credit.batch_number < 500)
        {
            log_ << "wallet is now: " << flexnode.wallet << "\n";
        }
        HandleNugget(msg.mined_credit);
        log_ << "Balance is now " << flexnode.wallet.Balance() << "\n";
        flexnode.pit.pool.already_included.insert(credit_hash);
        flexnode.digging = was_digging;
    }

    bool BatchChainer::CheckBadBlockMessage(BadBlockMessage msg)
    {
        MinedCreditMessage mined_credit_msg
            = creditdata[msg.message_hash]["msg"];

        if (msg.check.valid  ||
            msg.check.proof_hash != mined_credit_msg.proof.GetHash()  ||
            !msg.check.VerifyInvalid())
            return false;
        return true;
    }

    void BatchChainer::HandleBadBlockMessage(BadBlockMessage msg, CNode* peer)
    {
        if (!CheckBadBlockMessage(msg))
            return;
        SendBadBlockMessage(msg);
        MinedCredit credit = creditdata[msg.message_hash]["mined_credit"];
        uint160 credit_hash = credit.GetHash160();
        RemoveBlocksAndChildren(credit_hash);
        creditdata[credit_hash]["bad"] = true;
        creditdata[credit_hash]["check"] = msg.check;
        SwitchToBestChain();
    }

    void BatchChainer::SendBadBlockMessage(uint160 message_hash,
                                           TwistWorkCheck check)
    {
        BadBlockMessage msg(message_hash, check);
        SendBadBlockMessage(msg);
    }

    void BatchChainer::SendBadBlockMessage(BadBlockMessage msg)
    {
        foreach_(CNode* peer, vNodes)
        {
            peer->PushMessage("badblock", msg);
        }
    }


    void BatchChainer::HandleMinedCreditMessage(MinedCreditMessage msg, 
                                                CNode* peer)
    {
        log_ << "BatchChainer::HandleMinedCreditMessage(): msg is " << msg;
        uint160 credit_hash = msg.mined_credit.GetHash160();
        uint160 prev_hash = msg.mined_credit.previous_mined_credit_hash;

        if (msgdata[credit_hash]["processed"])
        {
            log_ << "already processed this message.\n";
            return;
        }
        if (!CheckMinedCreditForMissingData(msg, peer))
        {
            log_ << "HandleMinedCreditMessage(): missing data\n";
            StoreMinedCreditMessage(msg);
            flexnode.downloader.RequestMissingData(msg, peer);
            return;
        }

        if (!flexnode.message_validator.ValidateMinedCreditMessage(msg) || !msg.CheckHashList())
        {
            log_ << "Bad Mined Credit Message\n";
            return;
        }

        if (creditdata[prev_hash].HasProperty("mined_credit"))
        {
            MinedCredit prev_credit = creditdata[prev_hash]["mined_credit"];
            if (prev_credit.timestamp > msg.mined_credit.timestamp)
            {
                log_ << "timestamp earlier than that of previous credit\n";
                return;
            }
        }

        if (msg.mined_credit.timestamp > (uint64_t)GetTimeMicros())
        {
            log_ << "timestamp in future\n";
            return;
        }
        
        RelayMinedCredit(msg);

        TwistWorkCheck check = msg.proof.SpotCheck();
        bool result = check.valid;
        if (!result)
        {
            SendBadBlockMessage(msg.GetHash160(), check);
            return;
        }
        StoreMinedCreditMessage(msg);

        if (msg.proof.DifficultyAchieved() >= msg.mined_credit.difficulty)
        {
            log_ << "New Batch: " 
                 << msg.proof.DifficultyAchieved() << " vs "
                 << msg.mined_credit.difficulty << "\n";
            HandleBatch(msg, peer);
            CreditBatch batch = ReconstructBatch(msg);
            StoreRecipients(batch);
        }
        else
        {
            HandleNugget(msg.mined_credit);
        }
        msgdata[credit_hash]["handled"] = true;
        msgdata[credit_hash]["processed"] = true;
    }

    void BatchChainer::HandleNugget(MinedCredit mined_credit)
    {
        flexnode.pit.HandleNugget(mined_credit);
        vector<MinedCreditMessage> non_orphan_nuggets;
        non_orphan_nuggets = orphanage.NuggetNonOrphansAfterHash(
                                        mined_credit.GetHash160());
        foreach_(const MinedCreditMessage& msg_, non_orphan_nuggets)
        {
            log_ << "Handling orphan " << msg_;
            HandleMinedCreditMessage(msg_, NULL);
        }
    }

    bool BatchChainer::CheckTransactionForMissingData(SignedTransaction tx)
    {
        return tx.GetMissingCredits().size() == 0;
    }

    void BatchChainer::RequestMissingTransactionData(SignedTransaction tx,
                                                     CNode *peer)
    {
        if (peer == NULL)
            return;
        vector<uint160> missing_credits = tx.GetMissingCredits();
        log_ << "requesting missing diurn branches\n";
        foreach_(const uint160& credit_hash, missing_credits)
            initdata[credit_hash]["requested"] = true;
        if (missing_credits.size() > 0)
            peer->PushMessage("reqinitdata", "get_diurn_branches", 
                              missing_credits);
    }

    void BatchChainer::HandleDiurnBranchesReceived(vector<uint160> hashes,
                                                   CNode *peer)
    {
        foreach_(const uint160& credit_hash, hashes)
        {
            vector<SignedTransaction> txs
                = orphanage.TxNonOrphansAfterBatch(credit_hash);
            foreach_(SignedTransaction tx, txs)
                HandleTransaction(tx, peer);
        }
    }

    bool BatchChainer::DiurnsAreInCalendar(SignedTransaction tx)
    {
        foreach_(const CreditInBatch& credit, tx.rawtx.inputs)
        {
            if (credit.diurn_branch.size() != 1)
            {
                uint160 diurn_root = credit.diurn_branch.back();

                if (!flexnode.calendar.ContainsDiurn(diurn_root))
                {
                    log_ << "diurn root " << diurn_root 
                         << " is not in calendar\n";
                    return false;
                }
            }
        }
        return true;
    }

    SignedTransaction BatchChainer::AddMissingDiurnBranches(SignedTransaction tx)
    {
        foreach_(CreditInBatch& credit, tx.rawtx.inputs)
        {
            if (credit.diurn_branch.size() == 1
                && credit.branch.size() > 0)
            {
                uint160 mined_credit_hash
                    = SymmetricCombine(credit.diurn_branch[0],
                                       credit.branch.back());
                vector<uint160> branch = GetDiurnBranch(mined_credit_hash);
                if (branch.size() > 0)
                {
                    credit.diurn_branch.reserve(1 + branch.size());
                    credit.diurn_branch.insert(credit.diurn_branch.end(),
                                               branch.begin(), branch.end());
                }
            }
        }
        return tx;
    }

    void BatchChainer::HandleTransaction(SignedTransaction tx, CNode *peer)
    {
        log_ << "batchchain: got transaction " << tx << "\n";

        SignedTransaction tx_ = AddMissingDiurnBranches(tx);

        if (!DiurnsAreInCalendar(tx_))
        {
            log_ << "diurns are not in calendar\n";
            return;
        }
        
        if (!CheckTransactionForMissingData(tx_))
        {
            log_ << "missing data!\n";
            orphanage.AddTransaction(tx);
            RequestMissingTransactionData(tx_, peer);
            return;
        }

        if (!tx.Validate())
        {
            log_ << "invalid transaction!\n";
            return;
        }

        for (auto credit : tx_.rawtx.inputs)
        {
            if (!(flexnode.calendar.CreditInBatchHasValidConnection(credit)))
            {
                log_ << "transaction contains credits "
                     << "with no connection to calendar\n";
                return;
            }
        }

        StoreTransaction(tx);

        if (!TxContainsSpentInputs(tx))
        {
            log_ << "no double spends - sending to pit\n";
            flexnode.pit.HandleTransaction(tx);
            uint160 priority = tx.IncludedFees();
            outgoing_resource_limiter.ScheduleForwarding(tx.GetHash160(), 
                                                         priority);
        }
        
        log_ << "stored and forwarding scheduled. handling orphans\n";
        vector<MinedCreditMessage> non_orphans;
        non_orphans = orphanage.NuggetNonOrphansAfterHash(tx.GetHash160());
        foreach_(const MinedCreditMessage& msg, non_orphans)
            HandleMinedCreditMessage(msg, NULL);
    }

    bool BatchChainer::CheckMinedCreditForMissingData(MinedCreditMessage& msg,
                                                      CNode *peer)
    {
        if (!msg.hash_list.RecoverFullHashes())
        {
            log_ << "RecoverFullHashes failed!\n";
            log_ << "short hashes are:\n";
            foreach_(const uint32_t short_hash, msg.hash_list.short_hashes)
            {
                log_ << short_hash << "\n";
                std::vector<uint160> matches = hashdata[short_hash]["matches"];
                log_ << "matches: " << matches.size() << "\n";
                foreach_(const uint160 match, matches)
                    log_ << match << "\n";
            }

            log_ << "sending mined credit message to orphanage\n";
            orphanage.AddMinedCreditMessage(msg);
            return false;
        }
        else
        {
            log_ << "RecoverFullHashes succeeded for "
                 << msg.mined_credit.GetHash160() << "\n";
        }

        if (msg.mined_credit.batch_number == 1)
            return true;

        uint160 previous_hash = msg.mined_credit.previous_mined_credit_hash;
        RelayState previous_relay_state;
        try
        {
            previous_relay_state = GetRelayState(previous_hash);
        }
        catch (RecoverHashesFailedException& e)
        {
            return false;
        }
        catch (NoKnownRelayStateException& e)
        {
            return false;
        }

        MinedCreditMessage prev_msg = creditdata[previous_hash]["msg"];
        if (prev_msg.proof.DifficultyAchieved() 
                > prev_msg.mined_credit.diurnal_difficulty)
        {
            creditdata[previous_hash]["is_calend"] = true;
            vector<uint160> missing
                = MissingDataNeededToCalculateFees(previous_relay_state);
            if (missing.size() > 0)
                return false;
        }
        return true;
    }

/*
 * BatchChainer
 ****************/


