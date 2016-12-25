#ifndef TELEPORT_LOCATIONITERATOR_H
#define TELEPORT_LOCATIONITERATOR_H


#include <src/database/DiskDataStore.h>
#include "MemoryLocationIterator.h"

class LocationIterator
{
public:
    MemoryLocationIterator memory_location_iterator;
    DiskLocationIterator disk_location_iterator;
    bool using_memory{false};

    LocationIterator() { }

    LocationIterator(MemoryLocationIterator memory_location_iterator):
            memory_location_iterator(memory_location_iterator)
    { using_memory = true; }

    LocationIterator(DiskLocationIterator disk_location_iterator):
            disk_location_iterator(disk_location_iterator)
    {  using_memory = false; }

    template <typename V> void Seek(V location_value)
    {
        if (using_memory)
            memory_location_iterator.Seek(location_value);
        else
            disk_location_iterator.Seek(location_value);
    }

    void SeekStart()
    {
        if (using_memory)
            memory_location_iterator.SeekStart();
        else
            disk_location_iterator.SeekStart();
    }

    void SeekEnd()
    {
        if (using_memory)
            memory_location_iterator.SeekEnd();
        else
            disk_location_iterator.SeekEnd();
    }

    template <typename N, typename V> bool GetNextObjectAndLocation(N& objectname, V& location_value)
    {
        if (using_memory)
            return memory_location_iterator.GetNextObjectAndLocation(objectname, location_value);
        else
            return disk_location_iterator.GetNextObjectAndLocation(objectname, location_value);
    };

    template <typename N, typename V> bool GetPreviousObjectAndLocation(N& objectname, V& location_value)
    {
        if (using_memory)
            return memory_location_iterator.GetPreviousObjectAndLocation(objectname, location_value);
        else
            return disk_location_iterator.GetPreviousObjectAndLocation(objectname, location_value);
    };
};


#endif //TELEPORT_LOCATIONITERATOR_H
