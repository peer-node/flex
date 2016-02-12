#ifndef FLEX_MOCKDATASTORE_H
#define FLEX_MOCKDATASTORE_H

#include "MockObject.h"
#include "LocationIterator.h"
#include "MockDimension.h"

class MockDataStore : MockData
{
public:
    std::map<vch_t, MockObject> objects;
    std::map<vch_t, MockDimension> dimensions;

    template <typename OBJECT_NAME>
    MockObject& operator[](const OBJECT_NAME & object_name)
    {
        vch_t serialized_name = Serialize(object_name);

        if (not objects.count(serialized_name))
            objects[serialized_name] = MockObject(serialized_name, &dimensions);

        return objects[serialized_name];
    }

    MockObject& operator[](const char* object_name)
    {
        return (*this)[std::string(object_name)];
    }

    void Reset();

    template <typename LOCATION_NAME>
    ::LocationIterator LocationIterator(LOCATION_NAME location_name)
    {
        vch_t serialized_location_name = MockDataStore::Serialize(location_name);

        if (not dimensions.count(serialized_location_name))
            dimensions[serialized_location_name] = MockDimension();

        return ::LocationIterator(dimensions[serialized_location_name]);
    }

    ::LocationIterator LocationIterator(const char* location_name)
    {
        return LocationIterator(std::string(location_name));
    }
};


#endif //FLEX_MOCKDATASTORE_H
