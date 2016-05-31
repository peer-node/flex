#ifndef FLEX_MAINFLEXNODE_H
#define FLEX_MAINFLEXNODE_H

#include <src/net/net_cnode.h>
#include <src/base/sync.h>
#include <test/flex_tests/main_tests/CreditMessageHandler_.h>
#include "../../test/flex_tests/flex_data/TestData.h"


#include "log.h"
#define LOG_CATEGORY "MainFlexNode.h"

class CInv;

class MainFlexNode
{
public:
    bool HaveInventory(const CInv &inv);

    bool IsFinishedDownloading();

    void HandleMessage(std::string command, CDataStream &stream, CNode *peer);


    CreditMessageHandler_ *credit_message_handler{NULL};
    uint64_t messages_received{0};
    Mutex mutex;

    MemoryDataStore msgdata;
    MemoryDataStore guidata;
    MemoryDataStore scheduledata;
    MemoryDataStore currencydata;
    MemoryDataStore calendardata;
    MemoryDataStore commanddata;
    MemoryDataStore depositdata;
    MemoryDataStore marketdata;
    MemoryDataStore creditdata;
    MemoryDataStore walletdata;
    MemoryDataStore relaydata;
    MemoryDataStore tradedata;
    MemoryDataStore initdata;
    MemoryDataStore hashdata;
    MemoryDataStore taskdata;
    MemoryDataStore workdata;
    MemoryDataStore keydata;
};


#endif //FLEX_MAINFLEXNODE_H
