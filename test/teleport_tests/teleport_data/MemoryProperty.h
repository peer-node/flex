#ifndef TELEPORT_MEMORYPROPERTY_H
#define TELEPORT_MEMORYPROPERTY_H

#include "MemoryData.h"
#include <string>
#include <src/base/sync.h>

class MemoryProperty : MemoryData
{
public:
    vch_t serialized_value;
    Mutex mutex;

    MemoryProperty() { }

    MemoryProperty(const MemoryProperty& other)
    {
        serialized_value = other.serialized_value;
    }

    MemoryProperty& operator=(const MemoryProperty& other)
    {
        serialized_value = other.serialized_value;
        return *this;
    }

    template <typename VALUE> operator VALUE()
    {
        VALUE value;
        LOCK(mutex);
        Deserialize(serialized_value, value);
        return value;
    }

    template <typename VALUE>
    MemoryProperty& operator=(VALUE value)
    {
        LOCK(mutex);
        serialized_value = Serialize(value);
        return *this;
    }

    MemoryProperty& operator=(const char* value)
    {
        return (*this) = std::string(value);
    }
};


#endif //TELEPORT_MEMORYPROPERTY_H
