#ifndef TELEPORT_DATASTORE_H
#define TELEPORT_DATASTORE_H

#include <boost/any.hpp>
#include <src/database/DiskDataStore.h>
#include "DataObject.h"
#include "MemoryDataStore.h"
#include "LocationIterator.h"

class DataStore
{
public:
    MemoryDataStore *memory_datastore{NULL};
    DiskDataStore *disk_datastore{NULL};

    DataStore(MemoryDataStore *memory_datastore):
            memory_datastore(memory_datastore)
    { }

    DataStore(DiskDataStore *disk_datastore):
            disk_datastore(disk_datastore)
    { }

    template <typename T>
    DataObject operator[] (const T& object_name)
    {
        if (memory_datastore)
            return DataObject(memory_datastore, object_name);
        else
            return DataObject(disk_datastore, object_name);
    }

    template <typename LOCATION_NAME>
    ::LocationIterator GetLocationIterator(LOCATION_NAME location_name)
    {
        if (memory_datastore)
            return LocationIterator(memory_datastore->LocationIterator(location_name));
        return LocationIterator(disk_datastore->LocationIterator(location_name));
    }

    ::LocationIterator GetLocationIterator(const char *location_name)
    {
        if (memory_datastore)
            return LocationIterator(memory_datastore->LocationIterator(location_name));
        return LocationIterator(disk_datastore->LocationIterator(location_name));
    }
};


#endif //TELEPORT_DATASTORE_H
