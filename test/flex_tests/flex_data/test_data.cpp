#include "gmock/gmock.h"
#include "TestData.h"
#include "crypto/uint256.h"

using namespace ::testing;

class AMockDataStore : public Test
{
public:
    MockDataStore datastore;
};

class AMockObject : public Test
{
public:
    MockObject object;
};

class AMockProperty : public Test
{
public:
    MockProperty property;
};

TEST_F(AMockDataStore, ReturnsAMockObject)
{
    MockObject object;
    object = datastore[0];
}

TEST_F(AMockObject, ReturnsAMockProperty)
{
    MockProperty property;
    property = object[0];
}

TEST_F(AMockProperty, CanBeCastToDifferentTypes)
{
    int x;
    x = property;
}

TEST_F(AMockProperty, CanStoreInformation)
{
    int x = 5;
    property = x;
    int y;
    y = property;
    ASSERT_EQ(5, y);
}

TEST(AMockObjectsProperty, CanStoreInformation)
{
    MockObject object;
    object[0] = 5;
    int y;
    y = object[0];
    ASSERT_EQ(5, y);
}

TEST(AMockObjectsProperty, CanStoreRawStrings)
{
    MockObject object;
    object[0] = "hi there";
    std::string y = object[0];
    ASSERT_THAT(y, Eq(std::string("hi there")));
}

TEST_F(AMockObject, HasALocation)
{
    object.Location(7) = 10;
    int location = object.Location(7);
    ASSERT_THAT(location, Eq(10));
}

TEST_F(AMockObject, HasALocationWithARawStringName)
{
    object.Location("lattitude") = 5;
    int location = 0;
    location = object.Location("lattitude");
    ASSERT_THAT(location, Eq(5));
}

TEST_F(AMockDataStore, CanBeUsedToAssignProperties)
{
    datastore[0][0] = 5;
}

TEST_F(AMockDataStore, CanBeUsedToRetrieveProperties)
{
    int x = datastore[0][0];
}

TEST_F(AMockDataStore, RetrievesStoredData)
{
    datastore[0][0] = 4;
    int x = datastore[0][0];
    ASSERT_EQ(4, x);
}

TEST_F(AMockDataStore, CanAcceptRawStringsAsNames)
{
    MockObject object =  datastore["hat"];
}

TEST_F(AMockObject, CanAcceptRawStringsAsNames)
{
    MockProperty property = object["size"];
}

TEST_F(AMockDataStore, CanRetrieveDataStoredWithRawStringNames)
{
    datastore["hat"]["size"] = 25;
    int y = datastore["hat"]["size"];
    ASSERT_EQ(25, y);
}

TEST_F(AMockDataStore, ForgetsDataAfterReset)
{
    datastore["hat"]["size"] = 25;
    datastore.Reset();
    int y = datastore["hat"]["size"];
    ASSERT_EQ(0, y);
}

TEST_F(AMockDataStore, ReturnsALocationIterator)
{
    LocationIterator scanner;
    int x = 0;
    scanner = datastore.LocationIterator(x);
}

class ALocationIterator : public Test
{
public:
    MockDataStore datastore;
    LocationIterator scanner;
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

TEST_F(ALocationIterator, ReturnsObjectsAndLocations)
{
    scanner.GetNextObjectAndLocation(object, location);
}

TEST_F(AMockDataStore, ReturnsAnObjectWhoseDimensionsAreThoseOfTheDataStore)
{
    ASSERT_FALSE(datastore[100].using_internal_dimensions);
}

TEST_F(ALocationIterator, ReturnsTheNextObjectAndLocation)
{
    scanner.GetNextObjectAndLocation(object, location);
    ASSERT_THAT(object, Eq(100));
    ASSERT_THAT(location, Eq(200));
}

TEST_F(ALocationIterator, ReturnsObjectsAndLocationsInTheCorrectOrder)
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

TEST_F(ALocationIterator, ReturnsFalseWhenEndIsReached)
{
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetNextObjectAndLocation(object, location));
    ASSERT_FALSE(scanner.GetNextObjectAndLocation(object, location));
}

TEST_F(ALocationIterator, ReturnsObjectsAndLocationsInTheCorrectReverseOrder)
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


TEST_F(ALocationIterator, ReturnsFalseWhenStartIsReached)
{
    scanner.SeekEnd();
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_TRUE(scanner.GetPreviousObjectAndLocation(object, location));
    ASSERT_FALSE(scanner.GetPreviousObjectAndLocation(object, location));
}


TEST_F(ALocationIterator, CanBeResetToTheStartWithSeekStart)
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

TEST_F(AMockDataStore, CanRemoveAnObjectFromALocation)
{
    datastore[5].Location("lattitude") = 100;
    int location = datastore[5].Location("lattitude");

    ASSERT_THAT(location, Eq(100));

    datastore.RemoveFromLocation("lattitude", 100);
    location = datastore[5].Location("lattitude");

    ASSERT_THAT(location, Eq(0));
}

class ALocationIteratorWithUint160Locations : public Test
{
public:
    MockDataStore datastore;
    LocationIterator scanner;
    uint160 object;
    uint160 location;

    virtual void SetUp()
    {
        for (uint32_t i = 0; i < 160; i++)
        {
            object = 1;
            object = object << i;
            location = object;
            datastore[object].Location("lattitude") = location;
        }

        scanner = datastore.LocationIterator("lattitude");
    }
};

TEST_F(ALocationIteratorWithUint160Locations, ReturnsObjectsAndLocationsInTheCorrectOrder)
{
    uint32_t digits;
    for (digits = 0; digits < 160; digits++)
    {
        EXPECT_TRUE(scanner.GetNextObjectAndLocation(object, location)) << "failed at " << digits;
        EXPECT_TRUE(object == location);
        object = object >> digits;
        EXPECT_THAT(object, Eq(1)) << "failed at " << digits;
    }
    ASSERT_FALSE(scanner.GetNextObjectAndLocation(object, location));
}

class GlobalDatabases : public TestWithGlobalDatabases
{

};

TEST_F(GlobalDatabases, AreAccessibleInUnitTests)
{
    currencydata["hat"]["size"] = 25;
    int y = currencydata["hat"]["size"];
    ASSERT_EQ(25, y);
}

TEST_F(GlobalDatabases, DontRememberDataFromPreviousTests)
{
    int y = currencydata["hat"]["size"];
    ASSERT_EQ(0, y);
}