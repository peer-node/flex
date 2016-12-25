#include "gmock/gmock.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <src/database/DiskDataStore.h>
#include <boost/filesystem.hpp>

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ADiskDataStore : public Test
{
public:
    CLevelDBWrapper *wrapper;
    DiskDataStore datastore;
    string database_directory_name;

    virtual void SetUp()
    {
        database_directory_name = "tmpdir" + PrintToString(GetTimeMicros());
        wrapper = new CLevelDBWrapper(database_directory_name, 4096 * 4096);

        datastore = DiskDataStore(wrapper, "t");
        datastore.LocationMutex(0);
    }

    virtual void TearDown()
    {
        delete wrapper;
        boost::filesystem::remove_all(database_directory_name);
    }
};

TEST_F(ADiskDataStore, ReadsAndWrites)
{
    uint32_t x = 100, y = 110;

    datastore[x]["x"] = y;

    y = 5;

    y = datastore[x]["x"];
    ASSERT_THAT(y, Eq(110));
}

TEST_F(ADiskDataStore, DeletesObjectsAndTheirProperties)
{
    uint32_t x = 100, y = 110;

    datastore[x]["x"] = y;

    datastore.Delete(x);

    y = datastore[x]["x"];
    ASSERT_THAT(y, Eq(0));
}

TEST_F(ADiskDataStore, DeletesObjectsAndTheirLocations)
{
    datastore[0].Location("lattitude") = 4;
    datastore.Delete(0);

    int x = datastore[0].Location("lattitude");
    ASSERT_EQ(0, x);
}

class ADiskDataStorePopulatedByMultipleThreads : public ADiskDataStore
{
public:
    uint160 object, value;

    virtual void SetUp()
    {
        ADiskDataStore::SetUp();
        for (uint32_t i = 0; i < 160; i++)
        {
            value = object = ((uint160)1) << i;
            boost::thread thread(&ADiskDataStorePopulatedByMultipleThreads::StoreInDataBase, this, object, value);
        }
        MilliSleep(50);
    }

    virtual void StoreInDataBase(uint160 hash, uint160 value)
    {
        datastore[hash][hash] = value;
    }

    virtual void TearDown()
    {
        ADiskDataStore::TearDown();
    }
};

TEST_F(ADiskDataStorePopulatedByMultipleThreads, RetrievesTheDataCorrectly)
{
    for (uint32_t i = 0; i < 160; i++)
    {
        uint160 hash = 1;
        hash <<= i;
        uint160 value = datastore[hash][hash];
        EXPECT_THAT(value, Eq(hash)) << "failed at " << i;
    }
}

class ADiskDataStoreWithAnObjectWhosePropertiesArePopulatedByMultipleThreads :
        public ADiskDataStorePopulatedByMultipleThreads
{
public:
    uint160 object, value;

    virtual void SetUp()
    {
        ADiskDataStore::SetUp();
        for (uint32_t i = 0; i < 160; i++)
        {
            object = 1;
            object = object << i;
            value = object;
            boost::thread thread(&ADiskDataStoreWithAnObjectWhosePropertiesArePopulatedByMultipleThreads::StoreInDataBase,
                                 this, object, value);
        }
        MilliSleep(5);
    }

    virtual void StoreInDataBase(uint160 hash, uint160 value)
    {
        datastore["some_object"][hash] = value;
    }

    virtual void TearDown()
    {
        ADiskDataStore::TearDown();
    }
};

TEST_F(ADiskDataStoreWithAnObjectWhosePropertiesArePopulatedByMultipleThreads, RetrievesTheDataCorrectly)
{
    for (uint32_t i = 0; i < 160; i++)
    {
        uint160 property = 1;
        property <<= i;
        uint160 value = datastore["some_object"][property];
        EXPECT_THAT(value, Eq(property)) << "failed at " << i;
    }
}


class ADiskLocationIterator : public ADiskDataStore
{
public:
    DiskLocationIterator scanner;
    uint64_t object;
    uint64_t location;

    virtual void SetUp()
    {
        ADiskDataStore::SetUp();

        datastore[(uint64_t)200].Location("lattitude") = (uint64_t)300;
        datastore[(uint64_t)100].Location("lattitude") = (uint64_t)200;
        datastore[12345678900].Location("lattitude") = 12345678900;

        scanner = datastore.LocationIterator("lattitude");
    }

    virtual void TearDown()
    {
        ADiskDataStore::TearDown();
    }
};

TEST_F(ADiskLocationIterator, HasRetrievableLocations)
{
    uint64_t location_of_200 = datastore[(uint64_t)200].Location("lattitude");
    ASSERT_THAT(location_of_200, Eq(300));
}

TEST_F(ADiskLocationIterator, ReturnsObjectsAndLocations)
{
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
}

TEST_F(ADiskLocationIterator, ReturnsTheNextObjectAndLocation)
{
    scanner.GetNextObjectAndLocation(object, location);
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));
}

TEST_F(ADiskLocationIterator, DoesntReturnDeletedObjectsWhenReturningTheNextObjectAndLocation)
{
    datastore.Delete((uint64_t)100);
    scanner = datastore.LocationIterator("lattitude");   // need a new iterator after changing the data

    scanner.GetNextObjectAndLocation(object, location);
    ASSERT_THAT(object, Eq(200));
    ASSERT_THAT(location, Eq(300));
}

TEST_F(ADiskLocationIterator, ReturnsObjectsAndLocationsInTheCorrectOrder)
{
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(200));
    ASSERT_THAT(location, Eq(300));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(12345678900));
    ASSERT_THAT(location, Eq(12345678900));
}

TEST_F(ADiskLocationIterator, ReturnsFalseWhenEndIsReached)
{
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_FALSE(scanner.GetNextObjectAndLocation(object, location));
}

TEST_F(ADiskLocationIterator, ReturnsObjectsAndLocationsInTheCorrectReverseOrder)
{
    scanner.SeekEnd();
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(12345678900));
    ASSERT_THAT(location, Eq(12345678900));
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(200));
    ASSERT_THAT(location, Eq(300));
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));
}


TEST_F(ADiskLocationIterator, ReturnsFalseWhenStartIsReached)
{
    scanner.SeekEnd();
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_FALSE(scanner.GetPreviousObjectAndLocation(object, location));
}


TEST_F(ADiskLocationIterator, CanBeResetToTheStartWithSeekStart)
{
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_FALSE(scanner.GetNextObjectAndLocation(object, location));

    scanner.SeekStart();

    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_FALSE(scanner.GetNextObjectAndLocation(object, location));
}

TEST_F(ADiskLocationIterator, CanSeekToASpecificLocation)
{
    scanner.Seek(12345678899);
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(12345678900));
    ASSERT_THAT(location, Eq(12345678900));
    scanner.Seek((uint64_t)299);
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(200));
    ASSERT_THAT(location, Eq(300));
    scanner.Seek((uint64_t)199);
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));
}


class ADiskLocationIteratorWithUint160Locations : public ADiskDataStore
{
public:
    DiskLocationIterator scanner;
    uint160 object;
    uint160 location;

    virtual void SetUp()
    {
        ADiskDataStore::SetUp();

        for (uint32_t i = 0; i < 160; i++)
        {
            location = object = ((uint160)1) << i;
            datastore[object].Location("lattitude") = location;
        }

        scanner = datastore.LocationIterator("lattitude");
    }

    virtual void TearDown()
    {
        ADiskDataStore::TearDown();
    }
};

TEST_F(ADiskLocationIteratorWithUint160Locations, ReturnsObjectsAndLocationsInTheCorrectOrder)
{
    for (uint32_t digits = 0; digits < 160; digits++)
    {
        EXPECT_TRUE(scanner.GetNextObjectAndLocation(object, location)) << "failed at " << digits;
        EXPECT_TRUE(object == location);
        EXPECT_THAT(object >> digits, Eq(1)) << "failed at " << digits;
    }
    ASSERT_FALSE(scanner.GetNextObjectAndLocation(object, location));
}


class ADiskLocationIteratorWithUint160LocationsPopulatedByMultipleThreads : public ADiskDataStore
{
public:
    DiskLocationIterator scanner;
    uint160 object;
    uint160 location;

    virtual void SetUp()
    {
        ADiskDataStore::SetUp();
        for (uint32_t i = 0; i < 160; i++)
        {
            location = object = ((uint160)1) << i;
            boost::thread thread(&ADiskLocationIteratorWithUint160LocationsPopulatedByMultipleThreads::PutHashAtLocation,
                                 this, object, location);
        }
        MilliSleep(50);
    }

    void PutHashAtLocation(uint160 object, uint160 location)
    {
        datastore[object].Location("lattitude") = location;
    }

    virtual void TearDown()
    {
        ADiskDataStore::TearDown();
    }
};

TEST_F(ADiskLocationIteratorWithUint160LocationsPopulatedByMultipleThreads, ReturnsObjectsAndLocationsInTheCorrectOrder)
{
    scanner = datastore.LocationIterator("lattitude");
    for (uint32_t digits = 0; digits < 160; digits++)
    {
        EXPECT_TRUE(scanner.GetNextObjectAndLocation(object, location)) << "failed at " << digits;
        EXPECT_TRUE(object == location);
        EXPECT_THAT(object >> digits, Eq(1)) << "failed at " << digits;
    }
    ASSERT_FALSE(scanner.GetNextObjectAndLocation(object, location));
}

