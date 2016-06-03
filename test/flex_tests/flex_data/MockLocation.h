#ifndef FLEX_MOCKLOCATION_H
#define FLEX_MOCKLOCATION_H

#include "MockData.h"
#include "MockDimension.h"

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
