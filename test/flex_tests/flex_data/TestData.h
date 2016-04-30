#ifndef FLEX_TESTDATA_H
#define FLEX_TESTDATA_H

#include "gmock/gmock.h"
#include "MemoryDataStore.h"
#include "database/data.h"

class TestWithGlobalDatabases : public ::testing::Test
{
public:
    MemoryDataStore creditdata;

    virtual void TearDown() // delete contents of mock db after each test
    { }
};


#endif //FLEX_TESTDATA_H
