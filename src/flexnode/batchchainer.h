#ifndef FLEX_BATCHCHAINER
#define FLEX_BATCHCHAINER

#include "net/net.h"
#include "credits/creditutil.h"
#include "flexnode/orphanage.h"
#include "flexnode/wallet.h"

class BadBlockMessage
{
public:
    uint160 message_hash;
    TwistWorkCheck check;

    BadBlockMessage() { }

    BadBlockMessage(uint160 message_hash, TwistWorkCheck check):
        message_hash(message_hash), 
        check(check)
    { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(message_hash);
        READWRITE(check);
    )

    uint256 GetHash() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    };

    uint160 GetHash160() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    } 
};

std::vector<Credit> GetAllMinedCredits(MinedCreditMessage& msg);

std::vector<Credit> GetAllTransactionOutputs(MinedCreditMessage& msg);


class BatchChainer
{
public:
    Orphanage orphanage;
    uint160 total_work;
    
    BatchChainer() { total_work = 0; }

    void SwitchToBestChain();
    
    void RewindToCalend(uint160 calend_credit_hash);

    void SwitchToChainViaCalend(uint160 credit_hash);

    void SwitchToChainViaFork(uint160 root);

    void CheckForPaymentsToWatchedPubKeys(SignedTransaction tx,
                                          uint160 credit_hash);

    void AddBatchToTip(MinedCreditMessage& msg);

    void RemoveBatchFromTip();

    bool ValidateBatch(MinedCreditMessage& msg);

    void HandleBatch(MinedCreditMessage msg, CNode* peer);
    
    bool CheckBadBlockMessage(BadBlockMessage msg);

    void HandleBadBlockMessage(BadBlockMessage msg, CNode* peer);
    
    void SendBadBlockMessage(uint160 message_hash, TwistWorkCheck check);

    void SendBadBlockMessage(BadBlockMessage msg);

    bool CheckDiurnalDifficulty(MinedCreditMessage msg);

    bool CheckDifficulty(MinedCreditMessage msg);

    uint160 RecordTotalWork(MinedCreditMessage msg);
    
    void HandleNonOrphansAfterBatch(MinedCreditMessage msg);
    
    bool CheckMessageProof(MinedCreditMessage msg);

    void HandleMinedCreditMessage(MinedCreditMessage msg, CNode* peer);

    void HandleNewMostWorkBatch(MinedCreditMessage msg);

    void HandleNugget(MinedCredit mined_credit);
    
    std::vector<uint160> GetMissingCredits(SignedTransaction tx);
    
    bool DiurnsAreInCalendar(SignedTransaction tx);

    bool CheckTransactionForMissingData(SignedTransaction tx);
    
    void RequestMissingTransactionData(SignedTransaction tx, CNode *peer);
    
    void HandleDiurnBranchesReceived(vector<uint160> hashes, CNode *peer);
    
    SignedTransaction AddMissingDiurnBranches(SignedTransaction tx);

    bool ValidateTransaction(SignedTransaction tx);

    void HandleTransaction(SignedTransaction tx, CNode *peer);

    bool CheckMinedCreditForMissingData(MinedCreditMessage& msg, CNode *peer);
};

#endif