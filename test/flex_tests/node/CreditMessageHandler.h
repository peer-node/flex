#ifndef FLEX_CREDITMESSAGEHANDLER_H
#define FLEX_CREDITMESSAGEHANDLER_H


#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include "MessageHandlerWithOrphanage.h"
#include "CreditSystem.h"
#include "FlexConfig.h"
#include "MinedCreditMessageValidator.h"
#include "Calendar.h"
#include "TransactionValidator.h"
#include "BadBatchMessage.h"

class CreditMessageHandler : public MessageHandlerWithOrphanage
{
public:
    std::string channel{"credit"};

    MemoryDataStore &creditdata, &keydata;
    CreditSystem *credit_system;
    Calendar *calendar;
    BitChain *spent_chain;
    FlexConfig config;
    MinedCreditMessageValidator mined_credit_message_validator;
    TransactionValidator transaction_validator;
    std::vector<uint160> accepted_messages;
    std::set<uint64_t> positions_spent_by_accepted_transactions;

    CreditMessageHandler(MemoryDataStore &msgdata_,
                         MemoryDataStore &creditdata_,
                         MemoryDataStore &keydata_):
            MessageHandlerWithOrphanage(msgdata_),
            creditdata(creditdata_), keydata(keydata_)
    { }

    template <typename T>
    void Broadcast(T message)
    {
        if (network != NULL)
            network->Broadcast(channel, message.Type(), message);
    }

    void SetConfig(FlexConfig& config_) { config = config_; }

    void SetCalendar(Calendar& calendar_) { calendar = &calendar_; }

    void SetSpentChain(BitChain& spent_chain_) { spent_chain = &spent_chain_; }

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(MinedCreditMessage);
        HANDLESTREAM(SignedTransaction);
        HANDLESTREAM(BadBatchMessage);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(MinedCreditMessage);
        HANDLEHASH(SignedTransaction);
        HANDLEHASH(BadBatchMessage);
    }

    HANDLECLASS(MinedCreditMessage);
    HANDLECLASS(SignedTransaction);
    HANDLECLASS(BadBatchMessage);

    void HandleMinedCreditMessage(MinedCreditMessage msg);

    void HandleSignedTransaction(SignedTransaction tx);

    void HandleBadBatchMessage(BadBatchMessage bad_batch_message);

    void SetCreditSystem(CreditSystem *credit_system_);

    void RejectMessage(uint160 msg_hash);

    void FetchFailedListExpansion(uint160 msg_hash);

    void AddToTip(MinedCreditMessage &msg);

    bool CheckInputsAreUnspentAccordingToSpentChain(SignedTransaction tx);

    bool CheckInputsAreUnspentByAcceptedTransactions(SignedTransaction tx);

    bool CheckInputsAreOKAccordingToCalendar(SignedTransaction tx);

    void AcceptTransaction(SignedTransaction tx);

    bool PassesSpotCheckOfProofOfWork(MinedCreditMessage &msg);

    BadBatchMessage GetBadBatchMessage(uint160 msg_hash);

    void HandleValidMinedCreditMessage(MinedCreditMessage &msg);

    void RemoveFromMainChainAndSwitchToNewTip(uint160 credit_hash);

    void SwitchToNewTipIfAppropriate();

    void SwitchToTip(uint160 credit_hash);

    void SwitchToTipViaFork(uint160 credit_hash_of_new_tip);
};


#endif //FLEX_CREDITMESSAGEHANDLER_H
