#ifndef FLEX_TESTDATA_H
#define FLEX_TESTDATA_H

#include "gmock/gmock.h"
#include "MockDataStore.h"
#include "database/data.h"

class TestWithGlobalDatabases : public ::testing::Test
{
public:
    virtual void TearDown() // delete contents of mock db after each test
    {
        calendardata.Reset();
        currencydata.Reset();
        scheduledata.Reset();
        commanddata.Reset();
        depositdata.Reset();
        marketdata.Reset();
        walletdata.Reset();
        creditdata.Reset();
        relaydata.Reset();
        tradedata.Reset();
        hashdata.Reset();
        initdata.Reset();
        taskdata.Reset();
        workdata.Reset();
        guidata.Reset();
        keydata.Reset();
        msgdata.Reset();
    }
};


#endif //FLEX_TESTDATA_H
