#include <boost/thread.hpp>
#include <src/base/util_time.h>
#include <src/base/util_rand.h>
#include "gmock/gmock.h"
#include "crypto/uint256.h"
#include "MemoryDataStore.h"

using namespace ::testing;

class AMemoryDataStore : public Test
{
public:
    MemoryDataStore datastore;
};

class AMemoryObject : public Test
{
public:
    MemoryObject object;
};

class AMemoryProperty : public Test
{
public:
    MemoryProperty property;
};

TEST_F(AMemoryDataStore, ReturnsAMemoryObject)
{
    MemoryObject object;
    object = datastore[0];
}

TEST_F(AMemoryObject, ReturnsAMemoryProperty)
{
    MemoryProperty property;
    property = object[0];
}

TEST_F(AMemoryProperty, CanBeCastToDifferentTypes)
{
    int x;
    x = property;
}

TEST_F(AMemoryProperty, CanStoreInformation)
{
    int x = 5;
    property = x;
    int y;
    y = property;
    ASSERT_EQ(5, y);
}

TEST(AMemoryObjectsProperty, CanStoreInformation)
{
    MemoryObject object;
    object[0] = 5;
    int y;
    y = object[0];
    ASSERT_EQ(5, y);
}

TEST(AMemoryObjectsProperty, CanStoreRawStrings)
{
    MemoryObject object;
    object[0] = "hi there";
    std::string y = object[0];
    ASSERT_THAT(y, Eq(std::string("hi there")));
}

TEST_F(AMemoryObject, HasALocation)
{
    object.Location(7) = 10;
    int location = object.Location(7);
    ASSERT_THAT(location, Eq(10));
}

TEST_F(AMemoryObject, HasALocationWithARawStringName)
{
    object.Location("lattitude") = 5;
    int location = 0;
    location = object.Location("lattitude");
    ASSERT_THAT(location, Eq(5));
}

TEST_F(AMemoryDataStore, CanBeUsedToAssignProperties)
{
    datastore[0][0] = 5;
}

TEST_F(AMemoryDataStore, CanBeUsedToRetrieveProperties)
{
    int x = datastore[0][0];
}

TEST_F(AMemoryDataStore, RetrievesStoredData)
{
    datastore[0][0] = 4;
    int x = datastore[0][0];
    ASSERT_EQ(4, x);
}

TEST_F(AMemoryDataStore, DeletesObjectsAndTheirProperties)
{
    datastore[0][0] = 4;
    datastore.Delete(0);
    int x = datastore[0][0];
    ASSERT_EQ(0, x);
}

TEST_F(AMemoryDataStore, DeletesObjectsAndTheirLocations)
{
    datastore[0].Location("lattitude") = 4;
    datastore.Delete(0);
    log_ << "number of objects in datastore is " << datastore.objects.size() << "\n";
    MemoryObject o = datastore[0];
    int x = o.Location("lattitude");
    ASSERT_EQ(0, x);
}

TEST_F(AMemoryDataStore, CanAcceptRawStringsAsNames)
{
    MemoryObject object = datastore["hat"];
}

TEST_F(AMemoryObject, CanAcceptRawStringsAsNames)
{
    MemoryProperty property = object["size"];
}

TEST_F(AMemoryDataStore, CanRetrieveDataStoredWithRawStringNames)
{
    datastore["hat"]["size"] = 25;
    int y = datastore["hat"]["size"];
    ASSERT_EQ(25, y);
}

TEST_F(AMemoryDataStore, ForgetsDataAfterReset)
{
    datastore["hat"]["size"] = 25;
    datastore.Reset();
    int y = datastore["hat"]["size"];
    ASSERT_EQ(0, y);
}

TEST_F(AMemoryDataStore, ReturnsAMemoryLocationIterator)
{
    MemoryLocationIterator scanner;
    int x = 0;
    scanner = datastore.LocationIterator(x);
}

TEST_F(AMemoryDataStore, ReturnsTheObjectAtALocation)
{
    datastore["hat"].Location("body") = "head";
    std::string thing_on_head;
    datastore.GetObjectAtLocation(thing_on_head, "body", "head");
    ASSERT_THAT(thing_on_head, Eq("hat"));
}


class AMemoryDataStorePopulatedByMultipleThreads : public Test
{
public:
    MemoryDataStore datastore;
    uint160 object, value;

    virtual void SetUp()
    {
        for (uint32_t i = 0; i < 160; i++)
        {
            value = object = ((uint160)1) << i;
            boost::thread thread(&AMemoryDataStorePopulatedByMultipleThreads::StoreInDataBase, this, object, value);
        }
        MilliSleep(5);
    }

    virtual void StoreInDataBase(uint160 hash, uint160 value)
    {
        datastore[hash][hash] = value;
    }
};

TEST_F(AMemoryDataStorePopulatedByMultipleThreads, RetrievesTheDataCorrectly)
{
    for (uint32_t i = 0; i < 160; i++)
    {
        uint160 hash = 1;
        hash <<= i;
        uint160 value = datastore[hash][hash];
        EXPECT_THAT(value, Eq(hash)) << "failed at " << i;
    }
}

class AMemoryDataStoreWithAnObjectWhosePropertiesArePopulatedByMultipleThreads :
        public AMemoryDataStorePopulatedByMultipleThreads
{
public:
    MemoryDataStore datastore;
    uint160 object, value;

    virtual void SetUp()
    {
        for (uint32_t i = 0; i < 160; i++)
        {
            object = 1;
            object = object << i;
            value = object;
            boost::thread thread(&AMemoryDataStoreWithAnObjectWhosePropertiesArePopulatedByMultipleThreads::StoreInDataBase,
                                 this, object, value);
        }
        MilliSleep(5);
    }

    virtual void StoreInDataBase(uint160 hash, uint160 value)
    {
        datastore["some_object"][hash] = value;
    }
};

TEST_F(AMemoryDataStoreWithAnObjectWhosePropertiesArePopulatedByMultipleThreads, RetrievesTheDataCorrectly)
{
    for (uint32_t i = 0; i < 160; i++)
    {
        uint160 property = 1;
        property <<= i;
        uint160 value = datastore["some_object"][property];
        EXPECT_THAT(value, Eq(property)) << "failed at " << i;
    }
}

class AMemoryLocationIterator : public Test
{
public:
    MemoryDataStore datastore;
    MemoryLocationIterator scanner;
    uint64_t object;
    uint64_t location;

    virtual void SetUp()
    {
        datastore[(uint64_t)200].Location("lattitude") = (uint64_t)300;
        datastore[(uint64_t)100].Location("lattitude") = (uint64_t)200;
        datastore[12345678900].Location("lattitude") = 12345678900;

        scanner = datastore.LocationIterator("lattitude");
    }
};

TEST_F(AMemoryLocationIterator, ReturnsObjectsAndLocations)
{
    scanner.GetNextObjectAndLocation(object, location);
}

TEST_F(AMemoryDataStore, ReturnsAnObjectWhoseDimensionsAreThoseOfTheDataStore)
{
    ASSERT_FALSE(datastore[100].using_internal_dimensions);
}

TEST_F(AMemoryLocationIterator, ReturnsTheNextObjectAndLocation)
{
    scanner.GetNextObjectAndLocation(object, location);
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));
}

TEST_F(AMemoryLocationIterator, ReturnsObjectsAndLocationsInTheCorrectOrder)
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

TEST_F(AMemoryLocationIterator, ReturnsFalseWhenEndIsReached)
{
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_FALSE(scanner.GetNextObjectAndLocation(object, location));
}

TEST_F(AMemoryLocationIterator, ReturnsObjectsAndLocationsInTheCorrectReverseOrder)
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


TEST_F(AMemoryLocationIterator, ReturnsFalseWhenStartIsReached)
{
    scanner.SeekEnd();
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_FALSE(scanner.GetPreviousObjectAndLocation(object, location));
}


TEST_F(AMemoryLocationIterator, CanBeResetToTheStartWithSeekStart)
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

TEST_F(AMemoryLocationIterator, CanSeekToASpecificLocation)
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

TEST_F(AMemoryDataStore, CanRemoveAnObjectFromALocation)
{
    datastore[5].Location("lattitude") = 100;
    int location = datastore[5].Location("lattitude");

    ASSERT_THAT(location, Eq(100));

    datastore.RemoveFromLocation("lattitude", 100);
    location = datastore[5].Location("lattitude");

    ASSERT_THAT(location, Eq(0));
}

class AMemoryLocationIteratorWithUint160Locations : public Test
{
public:
    MemoryDataStore datastore;
    MemoryLocationIterator scanner;
    uint160 object;
    uint160 location;

    virtual void SetUp()
    {
        for (uint32_t i = 0; i < 160; i++)
        {
            location = object = ((uint160)1) << i;
            datastore[object].Location("lattitude") = location;
        }

        scanner = datastore.LocationIterator("lattitude");
    }
};

TEST_F(AMemoryLocationIteratorWithUint160Locations, ReturnsObjectsAndLocationsInTheCorrectOrder)
{
    for (uint32_t digits = 0; digits < 160; digits++)
    {
        EXPECT_TRUE(scanner.GetNextObjectAndLocation(object, location)) << "failed at " << digits;
        EXPECT_TRUE(object == location);
        EXPECT_THAT(object >> digits, Eq(1)) << "failed at " << digits;
    }
    ASSERT_FALSE(scanner.GetNextObjectAndLocation(object, location));
}


class AMemoryLocationIteratorWithUint160LocationsPopulatedByMultipleThreads :
        public AMemoryLocationIteratorWithUint160Locations
{
public:
    virtual void SetUp()
    {
        for (uint32_t i = 0; i < 160; i++)
        {
            location = object = ((uint160)1) << i;
            boost::thread thread(&AMemoryLocationIteratorWithUint160LocationsPopulatedByMultipleThreads::PutHashAtLocation,
                                 this, object, location);
        }
        MilliSleep(5);
    }

    void PutHashAtLocation(uint160 object, uint160 location)
    {
        datastore[object].Location("lattitude") = location;
    }
};

TEST_F(AMemoryLocationIteratorWithUint160LocationsPopulatedByMultipleThreads,
       ReturnsObjectsAndLocationsInTheCorrectOrder)
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

