#ifndef TELEPORT_DATALOCATION_H
#define TELEPORT_DATALOCATION_H

#include <src/database/DiskDataStore.h>
#include "MemoryLocation.h"

class DataLocation
{
public:
    MemoryLocation memory_location;
    DiskLocation disk_location;
    MemoryDimension dimension;
    bool using_memory_location{false};

    DataLocation(): memory_location(dimension) { }

    DataLocation(MemoryLocation memory_location): memory_location(memory_location)
    { using_memory_location = true; }

    DataLocation(DiskLocation disk_location): disk_location(disk_location), memory_location(dimension)
    { }

    template<typename T>
    DataLocation& operator=(const T& value)
    {
        if (using_memory_location)
            memory_location = value;
        else
            disk_location = value;
        return *this;
    }

    template <typename V>
    operator V() const
    {
        if (using_memory_location)
            return memory_location;
        return disk_location;
    }
};


#endif //TELEPORT_DATALOCATION_H
