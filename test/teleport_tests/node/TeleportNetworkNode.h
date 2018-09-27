#ifndef TELEPORT_TELEPORTNETWORKNODE_H
#define TELEPORT_TELEPORTNETWORKNODE_H


#include <test/teleport_tests/main_tests/Communicator.h>
#include <test/teleport_tests/node/relays/handlers/RelayMessageHandler.h>
#include "test/teleport_tests/node/calendar/Calendar.h"
#include "test/teleport_tests/node/wallet/Wallet.h"
#include "test/teleport_tests/node/credit/handlers/CreditMessageHandler.h"
#include "test/teleport_tests/node/historical_data/handlers/DataMessageHandler.h"
#include <test/teleport_tests/node/deposits/handlers/DepositMessageHandler.h>
#include "test/teleport_tests/node/credit/structures/CreditTracker.h"

#include "log.h"
#define LOG_CATEGORY "TeleportNetworkNode.h"


class TeleportNetworkNode
{
public:
    MemoryDataStore msgdata, creditdata, keydata, depositdata;
    Data data;
    Calendar calendar;
    Wallet *wallet{nullptr};
    CreditMessageHandler *credit_message_handler{nullptr};
    DataMessageHandler *data_message_handler{nullptr};
    RelayMessageHandler *relay_message_handler{nullptr};
    DepositMessageHandler *deposit_message_handler{nullptr};
    CreditSystem *credit_system{nullptr};
    BitChain spent_chain;
    Communicator *communicator{nullptr};
    TeleportConfig *config{nullptr};
    TipController tip_controller;
    MinedCreditMessageBuilder builder;

    TeleportNetworkNode():
            data(msgdata, creditdata, keydata, depositdata),
            tip_controller(data),
            builder(data)
    {
        credit_system = new CreditSystem(data);

        SetUpWallet();
        SetUpCreditMessageHandler();
        SetUpDataMessageHandler();
        SetUpRelayMessageHandler();
        SetUpDepositMessageHandler();
    }

    void SetConfig(TeleportConfig *config_)
    {
        config = config_;
        deposit_message_handler->SetConfig(*config);
    }

    void SetUpWallet()
    {
        wallet = new Wallet(data.keydata);
        wallet->SetCreditSystem(credit_system);
        wallet->SetSpentChain(&spent_chain);
        wallet->SetCalendar(&calendar);
    }

    void SetUpDepositMessageHandler()
    {
        deposit_message_handler = new DepositMessageHandler(data);
        deposit_message_handler->SetCalendar(calendar);
        deposit_message_handler->SetSpentChain(spent_chain);
        deposit_message_handler->SetMinedCreditMessageBuilder(&builder);
        deposit_message_handler->SetNetworkNode(this);
        deposit_message_handler->SetTeleportNetworkNode(this);
    }

    void SetUpRelayMessageHandler()
    {
        relay_message_handler = new RelayMessageHandler(data);
        relay_message_handler->SetCreditSystem(credit_system);
        relay_message_handler->SetCalendar(&calendar);
        relay_message_handler->SetMinedCreditMessageBuilder(&builder);
        relay_message_handler->SetTeleportNetworkNode(this);
    }

    void SetUpCreditMessageHandler()
    {
        credit_message_handler = new CreditMessageHandler(data);
        credit_message_handler->SetTipController(&tip_controller);
        credit_message_handler->SetMinedCreditMessageBuilder(&builder);
        credit_message_handler->SetCreditSystem(credit_system);
        credit_message_handler->SetCalendar(calendar);
        credit_message_handler->SetSpentChain(spent_chain);
        credit_message_handler->SetWallet(*wallet);
        credit_message_handler->SetNetworkNode(this);
        credit_message_handler->SetTeleportNetworkNode(this);
    }

    void SetUpDataMessageHandler()
    {
        data_message_handler = new DataMessageHandler(data);
        data_message_handler->SetCreditSystemAndGenerateHandlers(credit_system);
        data_message_handler->SetCalendar(&calendar);
        data_message_handler->SetNetworkNode(this);
        data_message_handler->SetTeleportNetworkNode(this);
    }

    ~TeleportNetworkNode()
    {
        delete deposit_message_handler;
        delete data_message_handler;
        delete credit_message_handler;
        delete relay_message_handler;
        delete credit_system;
        delete wallet;
    }

    Calendar GetCalendar();

    uint64_t Balance();

    void HandleMessage(std::string channel, CDataStream stream, CNode *peer);

    bool StartCommunicator();

    void StopCommunicator();

    MinedCreditMessage Tip();

    uint64_t LatestBatchNumber();

    MinedCreditMessage GenerateMinedCreditMessageWithoutProofOfWork();

    void HandleNewProof(NetworkSpecificProofOfWork proof);

    MinedCreditMessage RecallGeneratedMinedCreditMessage(uint256 credit_hash);

    uint160 SendCreditsToPublicKey(Point public_key, uint64_t amount);

    Point GetNewPublicKey();

    uint160 SendToAddress(std::string address, int64_t amount);

    void SwitchToNewCalendarSpentChainAndRelayState(Calendar new_calendar, BitChain spent_chain);

    void AttachCommunicatorNetworkToHandlers();

    void RecordReceiptOfMessage(std::string channel, CDataStream stream);

    std::string GetTeleportAddressFromPublicKey(Point public_key);

    Point GetPublicKeyFromTeleportAddress(std::string address);

    std::string GetCryptoCurrencyAddressFromPublicKey(std::string currency_code, Point public_key);

    uint64_t GetKnownPubKeyBalance(Point &pubkey);

    void HandleMessage(uint160 message_hash);

    void SwitchToNewCalendarSpentChainAndRelayState(Calendar new_calendar, BitChain new_spent_chain, RelayState &state);
};


#endif //TELEPORT_TELEPORTNETWORKNODE_H
