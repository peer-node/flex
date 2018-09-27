#ifndef TELEPORT_CREDITMESSAGEHANDLER_H
#define TELEPORT_CREDITMESSAGEHANDLER_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "test/teleport_tests/node/MessageHandlerWithOrphanage.h"
#include "test/teleport_tests/node/credit/structures/CreditSystem.h"
#include "test/teleport_tests/node/config/TeleportConfig.h"
#include "MinedCreditMessageValidator.h"
#include "test/teleport_tests/node/calendar/Calendar.h"
#include "TransactionValidator.h"
#include "test/teleport_tests/node/credit/messages/BadBatchMessage.h"
#include "test/teleport_tests/node/wallet/Wallet.h"
#include "test/teleport_tests/node/credit/messages/ListExpansionMessage.h"
#include "TipController.h"
#include "MinedCreditMessageBuilder.h"

class TeleportNetworkNode;

class CreditMessageHandler : public MessageHandlerWithOrphanage
{
public:
    Data data;
    MemoryDataStore &creditdata, &keydata;
    CreditSystem *credit_system{nullptr};
    Calendar *calendar{nullptr};
    Mutex calendar_mutex;
    BitChain *spent_chain{nullptr};
    Wallet *wallet{nullptr};
    TeleportNetworkNode *teleport_network_node{nullptr};
    TeleportConfig config;
    MinedCreditMessageValidator mined_credit_message_validator;
    TransactionValidator transaction_validator;

    TipController *tip_controller{nullptr};
    MinedCreditMessageBuilder *builder{nullptr};

    bool do_spot_checks{true}, using_internal_tip_controller{true},
            using_internal_builder{true}, send_join_messages{true};

    explicit CreditMessageHandler(Data data):
            data(data),
            MessageHandlerWithOrphanage(data.msgdata),
            creditdata(data.creditdata), keydata(data.keydata)
    {
        channel = std::string("credit");
        tip_controller = new TipController(data);
        builder = new MinedCreditMessageBuilder(data);
        tip_controller->SetMinedCreditMessageBuilder(builder);
    }

    ~CreditMessageHandler()
    {
        if (using_internal_tip_controller and tip_controller != nullptr)
            delete tip_controller;
        if (using_internal_builder and builder != nullptr)
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
        HANDLESTREAM(ListExpansionRequestMessage);
        HANDLESTREAM(ListExpansionMessage);
    }

    void HandleMessage(uint160 message_hash)
    {
        log_ << "CreditMessageHandler::HandleMessage " << message_hash << "\n";
        HANDLEHASH(MinedCreditMessage);
        HANDLEHASH(SignedTransaction);
        HANDLEHASH(ListExpansionRequestMessage);
        HANDLEHASH(ListExpansionMessage);
    }

    HANDLECLASS(MinedCreditMessage);
    HANDLECLASS(SignedTransaction);
    HANDLECLASS(ListExpansionRequestMessage);
    HANDLECLASS(ListExpansionMessage);

    void HandleMinedCreditMessage(MinedCreditMessage msg);

    void HandleSignedTransaction(SignedTransaction tx);

    void HandleListExpansionRequestMessage(ListExpansionRequestMessage request);

    void HandleListExpansionMessage(ListExpansionMessage message);

    void SetCreditSystem(CreditSystem *credit_system_);

    bool CheckInputsAreUnspentAccordingToSpentChain(SignedTransaction tx);

    bool CheckInputsAreUnspentByAcceptedTransactions(SignedTransaction tx);

    bool CheckInputsAreOKAccordingToCalendar(SignedTransaction tx);

    void AcceptTransaction(SignedTransaction tx);

    bool ProofOfWorkPassesSpotCheck(MinedCreditMessage &msg);

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

    void InformOtherMessageHandlersOfNewTip(MinedCreditMessage &msg);
};


#endif //TELEPORT_CREDITMESSAGEHANDLER_H
