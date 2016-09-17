#ifndef FLEX_MOCKLOCATION_H
#define FLEX_MOCKLOCATION_H

#include <src/base/sync.h>
#include "MockData.h"
#include "MockDimension.h"

#include "log.h"
#define LOG_CATEGORY "MockLocation.h"

class MockLocation : public MockData
{
public:
    MockDimension& dimension;
    vch_t serialized_object_name;
    vch_t serialized_location_value;

    MockLocation(vch_t serialized_object_name,
                 MockDimension& dimension,
                 vch_t serialized_location_value):
            dimension(dimension),
            serialized_object_name(serialized_object_name),
            serialized_location_value(serialized_location_value)
    { }

    MockLocation(const MockLocation& other):
            dimension(other.dimension),
            serialized_object_name(other.serialized_object_name),
            serialized_location_value(other.serialized_location_value)
    { }

    template<typename VALUE_TYPE>
    operator VALUE_TYPE()
    {
        VALUE_TYPE value;
        Deserialize(serialized_location_value, value);
        return value;
    }

    template<typename VALUE_TYPE>
    MockLocation& operator=(VALUE_TYPE value)
    {
        serialized_location_value = Serialize(value);
        LOCK(dimension.mutex);
        dimension.located_serialized_objects[serialized_location_value] = serialized_object_name;
        return *this;
    }

    MockLocation& operator=(const char* value)
    {
        *this = std::string(value);
        return *this;
    }
};


#endif //FLEX_MOCKLOCATION_H
