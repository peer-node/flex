#ifndef TELEPORT_CREDITMESSAGEHANDLER_H
#define TELEPORT_CREDITMESSAGEHANDLER_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "test/teleport_tests/node/MessageHandlerWithOrphanage.h"
#include "test/teleport_tests/node/CreditSystem.h"
#include "test/teleport_tests/node/config/TeleportConfig.h"
#include "MinedCreditMessageValidator.h"
#include "test/teleport_tests/node/calendar/Calendar.h"
#include "TransactionValidator.h"
#include "BadBatchMessage.h"
#include "test/teleport_tests/node/wallet/Wallet.h"
#include "ListExpansionMessage.h"
#include "TipController.h"
#include "MinedCreditMessageBuilder.h"

class TeleportNetworkNode;

class CreditMessageHandler : public MessageHandlerWithOrphanage
{
public:
    MemoryDataStore &creditdata, &keydata;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    Mutex calendar_mutex;
    BitChain *spent_chain{NULL};
    Wallet *wallet{NULL};
    TeleportNetworkNode *teleport_network_node{NULL};
    TeleportConfig config;
    MinedCreditMessageValidator mined_credit_message_validator;
    TransactionValidator transaction_validator;

    TipController *tip_controller{NULL};
    MinedCreditMessageBuilder *builder{NULL};

    bool do_spot_checks{true}, using_internal_tip_controller{true}, using_internal_builder{true};

    CreditMessageHandler(MemoryDataStore &msgdata_,
                         MemoryDataStore &creditdata_,
                         MemoryDataStore &keydata_):
            MessageHandlerWithOrphanage(msgdata_),
            creditdata(creditdata_), keydata(keydata_)
    {
        channel = std::string("credit");
        tip_controller = new TipController(msgdata, creditdata, keydata);
        builder = new MinedCreditMessageBuilder(msgdata, creditdata, keydata);
        tip_controller->SetMinedCreditMessageBuilder(builder);
    }

    ~CreditMessageHandler()
    {
        if (using_internal_tip_controller and tip_controller != NULL)
            delete tip_controller;
        if (using_internal_builder and builder != NULL)
            delete builder;
    }

    void SetTipController(TipController *tip_controller_);

    void SetMinedCreditMessageBuilder(MinedCreditMessageBuilder *builder_);

    void SetConfig(TeleportConfig& config_);

    void SetCalendar(Calendar& calendar_);

    void SetSpentChain(BitChain& spent_chain_);

    void SetWallet(Wallet& wallet_);

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

    bool CheckInputsAreUnspentAccordingToSpentChain(SignedTransaction tx);

    bool CheckInputsAreUnspentByAcceptedTransactions(SignedTransaction tx);

    bool CheckInputsAreOKAccordingToCalendar(SignedTransaction tx);

    void AcceptTransaction(SignedTransaction tx);

    bool ProofOfWorkPassesSpotCheck(MinedCreditMessage &msg);

    BadBatchMessage GetBadBatchMessage(uint160 msg_hash);

    void HandleValidMinedCreditMessage(MinedCreditMessage &msg);

    MinedCreditMessage Tip();

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
