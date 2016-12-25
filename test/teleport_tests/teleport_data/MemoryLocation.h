#ifndef TELEPORT_MEMORYLOCATION_H
#define TELEPORT_MEMORYLOCATION_H

#include <src/base/sync.h>
#include "MemoryData.h"
#include "MemoryDimension.h"

#include "log.h"
#define LOG_CATEGORY "MockLocation.h"

class MemoryLocation : public MemoryData
{
public:
    MemoryDimension& dimension;
    vch_t serialized_object_name;
    vch_t serialized_location_value;

    MemoryLocation(vch_t serialized_object_name,
                 MemoryDimension& dimension,
                 vch_t serialized_location_value):
            dimension(dimension),
            serialized_object_name(serialized_object_name),
            serialized_location_value(serialized_location_value)
    { }

    MemoryLocation(MemoryDimension &dimension): dimension(dimension) { }

    MemoryLocation(const MemoryLocation& other):
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
    MemoryLocation& operator=(VALUE_TYPE value)
    {
        serialized_location_value = Serialize(value);
        LOCK(dimension.mutex);
        dimension.located_serialized_objects[serialized_location_value] = serialized_object_name;
        return *this;
    }

    MemoryLocation& operator=(const char* value)
    {
        *this = std::string(value);
        return *this;
    }
};


#endif //TELEPORT_MEMORYLOCATION_H
