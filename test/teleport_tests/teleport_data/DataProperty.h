#ifndef TELEPORT_DATAPROPERTY_H
#define TELEPORT_DATAPROPERTY_H


#include <src/database/DiskDataStore.h>
#include "MemoryProperty.h"

class DataProperty
{
public:
    MemoryProperty *memory_property{NULL};
    DiskProperty disk_property;

    DataProperty(MemoryProperty& memory_property): memory_property(&memory_property) { }

    DataProperty(DiskProperty *disk_property): disk_property(*disk_property) { }

    template<typename T>
    DataProperty& operator=(const T& value)
    {
        if (memory_property)
            *memory_property = value;
        else
            disk_property = value;
        return *this;
    }

    template <typename V>
    operator V() const
    {
        if (memory_property)
            return *memory_property;
        return disk_property;
    }
};


#endif //TELEPORT_DATAPROPERTY_H
