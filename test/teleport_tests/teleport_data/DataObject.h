#ifndef TELEPORT_DATAOBJECT_H
#define TELEPORT_DATAOBJECT_H

#include <src/database/DiskDataStore.h>
#include "DataProperty.h"
#include "MemoryDataStore.h"
#include "DataLocation.h"


class DataObject
{
public:
    MemoryDataStore *memory_data_store{NULL};
    MemoryObject *memory_object{NULL};
    DiskDataStore *disk_data_store{NULL};
    DiskObject *disk_object{NULL};

    template <typename T>
    DataObject(MemoryDataStore *memory_data_store, T& object_name):
            memory_data_store(memory_data_store)
    {
        memory_object = &(*memory_data_store)[object_name];
    }

    template <typename T>
    DataObject(DiskDataStore *disk_data_store, T& object_name):
            disk_data_store(disk_data_store)
    {
        disk_object = new DiskObject(*disk_data_store, object_name);
    }

    ~DataObject()
    {
        if (disk_object)
            delete disk_object;
    }

    template<typename T> DataProperty operator[] (const T& property_name)
    {
        if (memory_data_store)
            return DataProperty((*memory_object)[property_name]);
        DiskProperty disk_property = (*disk_object)[property_name];
        DataProperty data_property(&disk_property);
        return data_property;
    }

    template<typename T>
    DataLocation Location(T location_name)
    {
        if (disk_object)
            return disk_object->Location(location_name);
        return memory_object->Location(location_name);
    }

    DataLocation Location(const char *location_name)
    {
        if (disk_object)
            return DataLocation(disk_object->Location(location_name));
        return DataLocation(memory_object->Location(location_name));
    }
};


#endif //TELEPORT_DATAOBJECT_H
