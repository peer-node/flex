#ifndef TELEPORT_TELEPORTNETWORKNODE_H
#define TELEPORT_TELEPORTNETWORKNODE_H


#include <test/teleport_tests/main_tests/Communicator.h>
#include <test/teleport_tests/node/relays/handlers/RelayMessageHandler.h>
#include "test/teleport_tests/node/calendar/Calendar.h"
#include "test/teleport_tests/node/wallet/Wallet.h"
#include "test/teleport_tests/node/credit/handlers/CreditMessageHandler.h"
#include "test/teleport_tests/node/historical_data/handlers/DataMessageHandler.h"
#include <test/teleport_tests/node/deposits/handlers/DepositMessageHandler.h>

#include "log.h"
#define LOG_CATEGORY "TeleportNetworkNode.h"


class TeleportNetworkNode
{
public:
    MemoryDataStore msgdata, creditdata, keydata, depositdata;
    Data data;
    Calendar calendar;
    Wallet *wallet{NULL};
    CreditMessageHandler *credit_message_handler{NULL};
    DataMessageHandler *data_message_handler{NULL};
    RelayMessageHandler *relay_message_handler{NULL};
    DepositMessageHandler *deposit_message_handler{NULL};
    CreditSystem *credit_system{NULL};
    BitChain spent_chain;
    Communicator *communicator{NULL};
    TeleportConfig *config{NULL};
    TipController tip_controller;
    MinedCreditMessageBuilder builder;

    TeleportNetworkNode():
            data(msgdata, creditdata, keydata, depositdata),
            tip_controller(data),
            builder(data)
    {
        credit_system = new CreditSystem(data.msgdata, data.creditdata);
        wallet = new Wallet(data.keydata);

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

    void SetUpDepositMessageHandler()
    {
        deposit_message_handler = new DepositMessageHandler(data);
        deposit_message_handler->SetCalendar(calendar);
        deposit_message_handler->SetSpentChain(spent_chain);
        deposit_message_handler->SetMinedCreditMessageBuilder(&builder);
        deposit_message_handler->SetNetworkNode(this);
    }

    void SetUpRelayMessageHandler()
    {
        relay_message_handler = new RelayMessageHandler(data);
        relay_message_handler->SetCreditSystem(credit_system);
        relay_message_handler->SetCalendar(&calendar);
        relay_message_handler->SetMinedCreditMessageBuilder(&builder);
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
    }

    void SetUpDataMessageHandler()
    {
        data_message_handler = new DataMessageHandler(msgdata, creditdata);
        data_message_handler->SetCreditSystemAndGenerateHandlers(credit_system);
        data_message_handler->SetCalendar(&calendar);
        data_message_handler->SetNetworkNode(this);
    }

    ~TeleportNetworkNode()
    {
        if (deposit_message_handler != NULL)
            delete deposit_message_handler;
        if (data_message_handler != NULL)
            delete data_message_handler;
        if (credit_message_handler != NULL)
            delete credit_message_handler;
        if (relay_message_handler != NULL)
            delete relay_message_handler;
        if (credit_system != NULL)
            delete credit_system;
        if (wallet != NULL)
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

    void SwitchToNewCalendarAndSpentChain(Calendar new_calendar, BitChain spent_chain);

    void AttachCommunicatorNetworkToHandlers();

    void RecordReceiptOfMessage(std::string channel, CDataStream stream);

    std::string GetTeleportAddressFromPublicKey(Point public_key);

    Point GetPublicKeyFromTeleportAddress(std::string address);

    std::string GetCryptoCurrencyAddressFromPublicKey(std::string currency_code, Point public_key);
};


#endif //TELEPORT_TELEPORTNETWORKNODE_H
