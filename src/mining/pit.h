// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef TELEPORT_PIT
#define TELEPORT_PIT

#include "credits/credits.h"
#include "database/memdb.h"
#include "teleportnode/wallet.h"
#include "net/net.h"
#include "mining/work.h"
#include <boost/foreach.hpp>
#include "credits/creditsign.h"
#include "relays/relaystate.h"

static TwistWorkProof new_proof;



class MemPool
{
public:
    std::set<uint160> transactions;
    std::map<uint160, std::set<uint160> > multiple_spends;
    std::set<uint160> mined_credits;
    std::set<uint160> already_included;

    void CountDoubleSpend(SignedTransaction tx);

    bool CheckForDroppedTransactions(uint160 credit_hash,
                                     std::vector<uint160> missing_txs,
                                     std::vector<uint160> extra_txs, 
                                     std::vector<uint160>& dropped_txs);

    bool ChangeChain(uint160 from_hash, uint160 to_hash, 
                     std::vector<uint160>& dropped_txs);

    void AddTransaction(SignedTransaction tx);

    void AddMinedCredit(MinedCredit mined_credit);

    bool ContainsTransaction(uint160 txhash);

    bool ContainsMinedCredit(uint160 mined_credit_hash);

    bool Contains(uint160 hash);

    bool IsDoubleSpend(uint160 hash);

    void RemoveTransaction(uint160 txhash);

    void RemoveMinedCredit(uint160 mined_credit_hash);

    void Remove(uint160 hash);

    void RemoveMany(std::vector<uint160> hashes);
};

uint256 MemorySeedFromMinedCredit(MinedCredit mined_credit);


class CreditPit
{
public:
    MemPool pool;
    CreditBatch batch;
    
    std::set<uint160> accepted_txs;
    std::set<uint160> accepted_mined_credits;
    std::vector<uint160> accepted_relay_msgs;
    std::set<uint160> encoded_in_chain;
    std::set<uint160> in_current_batch;
    std::vector<uint160> mined_credit_contents;

    TwistWorkProof proof;
    MinedCredit mined_credit;
    uint64_t hashrate;
    
    bool started;

    CreditPit();

    void Initialize();

    void ResetBatch();

    void MarkAsEncoded(uint160 hash);

    void MarkAsUnencoded(uint160 hash);

    bool IsAlreadyEncoded(uint160 hash);

    void RemoveFromAccepted(std::vector<uint160> hashes);

    uint256 SeedMemory();

    void HandleNugget(MinedCredit mined_credit);
    
    void PrepareMinedCredit();

    void SetMinedCreditHashListHash();

    void SetMinedCreditContents();

    void HandleTransaction(SignedTransaction tx);

    void AddTransactionToCurrentBatch(SignedTransaction tx);

    void HandleRelayJoin(uint160 join_msg_hash);

    void HandleRelayFutureSuccessor(uint160 msg_hash);

    void HandleRelayMessage(uint160 msg_hash);

    uint160 LinkThreshold();

    uint160 Target();

    void AddBatchToTip(MinedCreditMessage& msg);

    bool SwitchToNewChain(MinedCreditMessage msg, CNode* pfrom);

    void SwitchToNewCalend(uint160 calend_credit_hash);
    
    void AddAccepted();
    
    void AddFees();

    void StartMining();

    void HandleNewProof();

    void MineForCredits();

    void StopMining();

    void SendTransaction(SignedTransaction tx);
};


#endif