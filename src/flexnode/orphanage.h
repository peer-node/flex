#ifndef FLEX_ORPHANAGE
#define FLEX_ORPHANAGE

#include "credits/credits.h"
#include "database/memdb.h"
#include "database/data.h"
#include "net/net.h"

#include "log.h"
#define LOG_CATEGORY "orphanage.h"


class Orphanage
{
public:
    CMemoryDB txorphandb;
    CMemoryDB nuggetorphandb;
    CMemoryDB missing_parent_counts;

    bool BatchIsKnown(uint160 credit_hash)
    {
        if (credit_hash == 0)
            return true;
        return creditdata[credit_hash].HasProperty("mined_credit")
               || creditdata[credit_hash].HasProperty("branch");
    }

    bool IsOrphan(uint160 hash)
    {
        int32_t num_parents = missing_parent_counts[hash];
        return num_parents > 0;
    }

    void IncrementNumParents(uint160 hash)
    {
        int32_t num_parents = missing_parent_counts[hash];
        num_parents += 1;
        missing_parent_counts[hash] = num_parents;
    }

    bool DecrementNumParentsAndCheckStillOrphan(uint160 hash)
    {
        int32_t num_parents = missing_parent_counts[hash];
        num_parents -= 1;
        missing_parent_counts[hash] = num_parents;
        log_ << "DecrementNumParentsAndCheckStillOrphan: "
             << hash << " now has " << num_parents << " missing parents\n";
        return num_parents > 0;
    }

    int32_t AddMinedCreditMessage(MinedCreditMessage& msg) 
    {
        uint160 prev_hash = msg.mined_credit.previous_mined_credit_hash;

        if (!BatchIsKnown(prev_hash))
        {
            log_ << "orphanage: batch is missing previous credit: "
                 << prev_hash << "\n";
            std::vector<MinedCreditMessage> nugget_orphans = nuggetorphandb[prev_hash];
            nugget_orphans.push_back(msg);
            nuggetorphandb[prev_hash] = nugget_orphans;
            IncrementNumParents(msg.GetHash160());
        }
        foreach_(const uint32_t& hash, msg.hash_list.short_hashes)
        {
            if (!memcmp(&hash, &prev_hash, 4))
                continue;

            std::vector<uint160> matches = hashdata[hash]["matches"];

            if (matches.size() == 0)
            {
                log_ << "orphanage: missing " << hash << "\n";
                std::vector<MinedCreditMessage> nugget_orphans = nuggetorphandb[hash];
                if (!VectorContainsEntry(nugget_orphans, msg))
                {
                    nugget_orphans.push_back(msg);
                    IncrementNumParents(msg.GetHash160());
                }
                nuggetorphandb[hash] = nugget_orphans;
                
            }
        }
        int32_t num_parents = missing_parent_counts[msg.GetHash160()];
        log_ << "orphanage::AddMinedCreditMessage: "
             << msg.mined_credit.GetHash160()
             << " has " << num_parents << " missing parents\n";
        return num_parents;
    }

    void AddTransaction(SignedTransaction& tx)
    {
        uint160 txhash = tx.GetHash160();

        std::set<uint160> missing_credits;
        foreach_(const CreditInBatch& credit, tx.rawtx.inputs)
        {
            uint160 batch_root = credit.branch.back();
            uint160 branch_bridge = credit.diurn_branch[0];
            uint160 mined_credit_hash = SymmetricCombine(batch_root,
                                                              branch_bridge);

            if (!BatchIsKnown(mined_credit_hash))
            {
                if (missing_credits.count(mined_credit_hash))
                    continue;
                missing_credits.insert(mined_credit_hash);
                std::vector<SignedTransaction> tx_orphans
                    = txorphandb[mined_credit_hash];
                tx_orphans.push_back(tx);
                txorphandb[mined_credit_hash] = tx_orphans;
                IncrementNumParents(txhash);
            }
        }
    }

    std::vector<SignedTransaction> TxNonOrphansAfterBatch(uint160 credit_hash)
    {
        std::vector<SignedTransaction> old_orphans = txorphandb[credit_hash];
        std::vector<SignedTransaction> non_orphans;
        txorphandb.Erase(credit_hash);

        foreach_(SignedTransaction& tx, old_orphans)
        {
            if (!DecrementNumParentsAndCheckStillOrphan(tx.GetHash160()))
            {
                non_orphans.push_back(tx);
            }
        }
        return non_orphans;
    }

    std::vector<MinedCreditMessage> 
    NuggetNonOrphansAfterBatch(uint160 credit_hash)
    {
        std::vector<MinedCreditMessage> old_orphans = nuggetorphandb[credit_hash];
        std::vector<MinedCreditMessage> non_orphans;
        
        nuggetorphandb.Erase(credit_hash);

        foreach_(const MinedCreditMessage& msg, old_orphans)
        {
            log_ << "NuggetNonOrphansAfterBatch: orphan is "
                 << msg.mined_credit.GetHash160() << "\n";
            if (!DecrementNumParentsAndCheckStillOrphan(msg.GetHash160()))
            {
                non_orphans.push_back(msg);
            }
        }
        return non_orphans;
    }

    std::vector<MinedCreditMessage> NuggetNonOrphansAfterHash(uint160 hash_)
    {
        uint32_t hash = *(uint32_t*)&hash_;
        std::vector<MinedCreditMessage> old_orphans = nuggetorphandb[hash];
        std::vector<MinedCreditMessage> non_orphans;
        nuggetorphandb.Erase(hash);

        foreach_(const MinedCreditMessage& msg, old_orphans)
        {
            if (!DecrementNumParentsAndCheckStillOrphan(msg.GetHash160()))
            {
                non_orphans.push_back(msg);
            }
        }
        return non_orphans;
    }

};

#endif