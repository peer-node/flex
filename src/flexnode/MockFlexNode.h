#ifndef FLEX_MOCKFLEXNODE_H
#define FLEX_MOCKFLEXNODE_H

#include "../../test/flex_tests/flex_data/TestData.h"


#include "log.h"
#define LOG_CATEGORY "flexnode.h"


class MockFlexNode
{
public:
    bool HaveInventory(const CInv &inv);

    bool IsFinishedDownloading();

    void LockMutex();

    void HandleMessage(string command, CDataStream &stream, CNode *peer);
    
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

bool MockFlexNode::HaveInventory(const CInv &inv)
{
    return false;
}

bool MockFlexNode::IsFinishedDownloading()
{
    return false;
}

void MockFlexNode::LockMutex()
{

}

void MockFlexNode::HandleMessage(string command, CDataStream &stream, CNode *peer)
{

}


extern MockFlexNode flexnode;

#endif //FLEX_MOCKFLEXNODE_H
