#include "gmock/gmock.h"
#include "MemoryDataStore.h"
#include <src/database/DiskDataStore.h>
#include <boost/filesystem.hpp>
#include "DataStore.h"

#include "log.h"
#define LOG_CATEGORY "test"

using namespace ::testing;
using namespace std;



void StoreSomeData(DataStore store)
{
    int i = 5, j = 6;

    store[i]["x"] = j;
}

bool DataCanBeRetrieved(DataStore store)
{
    int i = 5, j = 0;
    j = store[i]["x"];
    return j == 6;
}


class ADataStore : public Test
{
public:
    CLevelDBWrapper *wrapper;
    DiskDataStore disk_data_store;
    MemoryDataStore memory_data_store;
    std::string disk_data_directory;

    virtual void SetUp()
    {
        disk_data_directory = "x" + PrintToString(rand());
        wrapper = new CLevelDBWrapper(disk_data_directory, 4096 * 4096);
        disk_data_store = DiskDataStore(wrapper, "x");
    }

    virtual void TearDown()
    {
        delete wrapper;
        boost::filesystem::remove_all(disk_data_directory);
    }
};

TEST_F(ADataStore, CanStoreAndRetrieveDataFromDiskOrMemory)
{
    StoreSomeData(DataStore(&disk_data_store));
    StoreSomeData(DataStore(&memory_data_store));

    ASSERT_TRUE(DataCanBeRetrieved(DataStore(&disk_data_store)));
    ASSERT_TRUE(DataCanBeRetrieved(DataStore(&memory_data_store)));
}


class ALocationIteratorFromADataStore : public ADataStore
{
public:
    LocationIterator memory_scanner;
    LocationIterator disk_scanner;
    uint64_t object;
    uint64_t location;

    virtual void SetUp()
    {
        ADataStore::SetUp();
        DataStore memory_datastore(&memory_data_store);
        DataStore disk_datastore(&disk_data_store);
        
        PopulateSomeLocationData(memory_datastore);
        PopulateSomeLocationData(disk_datastore);
        
        memory_scanner = memory_datastore.GetLocationIterator("lattitude");
        disk_scanner = disk_datastore.GetLocationIterator("lattitude");
    }
    
    void PopulateSomeLocationData(DataStore datastore)
    {
        datastore[(uint64_t)200].Location("lattitude") = (uint64_t)300;
        datastore[(uint64_t)100].Location("lattitude") = (uint64_t)200;
        datastore[12345678900].Location("lattitude") = 12345678900;
    }

    virtual void TearDown()
    {
        ADataStore::TearDown();
    }
};

TEST_F(ALocationIteratorFromADataStore, ReturnsObjectsAndLocations)
{
    ASSERT_TRUE(memory_scanner.GetNextObjectAndLocation(object, location));

    ASSERT_TRUE(disk_scanner.GetNextObjectAndLocation(object, location));
}

TEST_F(ALocationIteratorFromADataStore, ReturnsTheNextObjectAndLocation)
{
    memory_scanner.GetNextObjectAndLocation(object, location);
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));

    disk_scanner.GetNextObjectAndLocation(object, location);
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));
}

TEST_F(ALocationIteratorFromADataStore, ReturnsObjectsAndLocationsInTheCorrectOrder)
{
    ASSERT_TRUE(memory_scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));
    ASSERT_TRUE(memory_scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(200));
    ASSERT_THAT(location, Eq(300));
    ASSERT_TRUE(memory_scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(12345678900));
    ASSERT_THAT(location, Eq(12345678900));

    ASSERT_TRUE(disk_scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));
    ASSERT_TRUE(disk_scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(200));
    ASSERT_THAT(location, Eq(300));
    ASSERT_TRUE(disk_scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(12345678900));
    ASSERT_THAT(location, Eq(12345678900));
}