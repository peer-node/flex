#ifndef TELEPORT_CREDITMESSAGEHANDLER_H
#define TELEPORT_CREDITMESSAGEHANDLER_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "MessageHandlerWithOrphanage.h"
#include "CreditSystem.h"
#include "TeleportConfig.h"
#include "MinedCreditMessageValidator.h"
#include "Calendar.h"
#include "TransactionValidator.h"
#include "BadBatchMessage.h"
#include "Wallet.h"
#include "ListExpansionMessage.h"

class TeleportNetworkNode;

class CreditMessageHandler : public MessageHandlerWithOrphanage
{
public:
    MemoryDataStore &creditdata, &keydata;
    CreditSystem *credit_system;
    Calendar *calendar;
    Mutex calendar_mutex;
    BitChain *spent_chain;
    Wallet *wallet{NULL};
    TeleportNetworkNode *teleport_network_node{NULL};
    TeleportConfig config;
    MinedCreditMessageValidator mined_credit_message_validator;
    TransactionValidator transaction_validator;
    std::vector<uint160> accepted_messages;
    bool do_spot_checks{true};

    CreditMessageHandler(MemoryDataStore &msgdata_,
                         MemoryDataStore &creditdata_,
                         MemoryDataStore &keydata_):
            MessageHandlerWithOrphanage(msgdata_),
            creditdata(creditdata_), keydata(keydata_)
    {
        channel = std::string("credit");
    }

    void SetConfig(TeleportConfig& config_) { config = config_; }

    void SetCalendar(Calendar& calendar_) { calendar = &calendar_; }

    void SetSpentChain(BitChain& spent_chain_) { spent_chain = &spent_chain_; }

    void SetWallet(Wallet& wallet_) { wallet = &wallet_; }

    void SetNetworkNode(TeleportNetworkNode *teleport_network_node_);

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(MinedCreditMessage);
        HANDLESTREAM(SignedTransaction);
        HANDLESTREAM(BadBatchMessage);
        HANDLESTREAM(ListExpansionRequestMessage);
        HANDLESTREAM(ListExpansionMessage);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(MinedCreditMessage);
        HANDLEHASH(SignedTransaction);
        HANDLEHASH(BadBatchMessage);
        HANDLEHASH(ListExpansionRequestMessage);
        HANDLEHASH(ListExpansionMessage);
    }

    HANDLECLASS(MinedCreditMessage);
    HANDLECLASS(SignedTransaction);
    HANDLECLASS(BadBatchMessage);
    HANDLECLASS(ListExpansionRequestMessage);
    HANDLECLASS(ListExpansionMessage);

    void HandleMinedCreditMessage(MinedCreditMessage msg);

    void HandleSignedTransaction(SignedTransaction tx);

    void HandleBadBatchMessage(BadBatchMessage bad_batch_message);

    void HandleListExpansionRequestMessage(ListExpansionRequestMessage request);

    void HandleListExpansionMessage(ListExpansionMessage message);

    void SetCreditSystem(CreditSystem *credit_system_);

    void AddToTip(MinedCreditMessage &msg);

    bool CheckInputsAreUnspentAccordingToSpentChain(SignedTransaction tx);

    bool CheckInputsAreUnspentByAcceptedTransactions(SignedTransaction tx);

    bool CheckInputsAreOKAccordingToCalendar(SignedTransaction tx);

    void AcceptTransaction(SignedTransaction tx);

    bool ProofOfWorkPassesSpotCheck(MinedCreditMessage &msg);

    BadBatchMessage GetBadBatchMessage(uint160 msg_hash);

    void HandleValidMinedCreditMessage(MinedCreditMessage &msg);

    void RemoveFromMainChainAndSwitchToNewTip(uint160 credit_hash);

    void SwitchToNewTipIfAppropriate();

    void SwitchToTip(uint160 credit_hash);

    void SwitchToTipViaFork(uint160 credit_hash_of_new_tip);

    void UpdateAcceptedMessagesAfterNewTip(MinedCreditMessage &msg);

    MinedCreditMessage Tip();

    void UpdateAcceptedMessagesAfterFork(uint160 old_tip, uint160 new_tip);

    void UpdateAcceptedMessagesAfterFork(std::vector<uint160> messages_on_old_branch,
                                         std::vector<uint160> messages_on_new_branch);

    std::set<uint64_t> PositionsSpentByAcceptedTransactions();

    void ValidateAcceptedMessagesAfterFork();

    bool TransactionHasNoSpentInputs(SignedTransaction tx, std::set<uint160> &spent_positions);

    MinedCreditMessage GenerateMinedCreditMessageWithoutProofOfWork();

    bool MinedCreditMessagePassesVerification(MinedCreditMessage &msg);

    bool EnclosedMessagesArePresent(MinedCreditMessage msg);

    void FetchFailedListExpansion(MinedCreditMessage msg);

    bool ValidateListExpansionMessage(ListExpansionMessage list_expansion_message);

    void HandleMessagesInListExpansionMessage(ListExpansionMessage list_expansion_message);

    void HandleMessageWithSpecifiedTypeAndContent(std::string type, vch_t content, CNode *peer);

    bool PreviousMinedCreditMessageWasHandled(MinedCreditMessage &msg);

    void QueueMinedCreditMessageBehindPrevious(MinedCreditMessage &msg);

    void HandleQueuedMinedCreditMessages(MinedCreditMessage &msg);

    void HandleMinedCreditMessageWithGivenListExpansion(ListExpansionMessage list_expansion_message);

    bool MinedCreditMessageWasRegardedAsValidAndHandled(MinedCreditMessage &msg);
};


#endif //TELEPORT_CREDITMESSAGEHANDLER_H
