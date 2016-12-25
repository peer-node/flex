#ifndef TELEPORT_MEMORYDATASTORE_H
#define TELEPORT_MEMORYDATASTORE_H

#include "MemoryObject.h"
#include "MemoryLocationIterator.h"
#include "MemoryDimension.h"

#include "log.h"
#define LOG_CATEGORY "MemoryDataStore.h"

class MemoryDataStore : MemoryData
{
public:
    std::map<vch_t, MemoryObject> objects;
    std::map<vch_t, MemoryDimension> dimensions;
    Mutex objects_mutex, dimensions_mutex;

    MemoryDataStore() { }

    MemoryDataStore(const MemoryDataStore& other)
    {
        objects = other.objects;
        dimensions = other.dimensions;
    }

    MemoryDataStore& operator=(const MemoryDataStore& other)
    {
        objects = other.objects;
        dimensions = other.dimensions;
        return *this;
    }

    template <typename OBJECT_NAME>
    MemoryObject& operator[](const OBJECT_NAME& object_name)
    {
        vch_t serialized_name = Serialize(object_name);

        LOCK(objects_mutex);
        if (not objects.count(serialized_name))
            objects[serialized_name] = MemoryObject(serialized_name, &dimensions);

        return objects[serialized_name];
    }

    MemoryObject& operator[](const char* object_name)
    {
        return (*this)[std::string(object_name)];
    }

    template <typename OBJECT_NAME>
    void Delete(const OBJECT_NAME& object_name)
    {
        vch_t serialized_object_name = Serialize(object_name);
        DeleteFromObjects(serialized_object_name);
        DeleteFromLocations(serialized_object_name);
    }

    void DeleteFromObjects(vch_t& serialized_object_name)
    {
        LOCK(objects_mutex);
        objects.erase(serialized_object_name);
    }

    void DeleteFromLocations(vch_t& serialized_object_name)
    {
        LOCK(dimensions_mutex);
        for (auto &item : dimensions)
        {
            item.second.Delete(serialized_object_name);
        }
    }

    void Reset();

    template <typename LOCATION_NAME>
    ::MemoryLocationIterator LocationIterator(LOCATION_NAME location_name)
    {
        vch_t serialized_location_name = MemoryDataStore::Serialize(location_name);

        LOCK(dimensions_mutex);
        if (not dimensions.count(serialized_location_name))
            dimensions[serialized_location_name] = MemoryDimension();

        return ::MemoryLocationIterator(dimensions[serialized_location_name]);
    }

    ::MemoryLocationIterator LocationIterator(const char* location_name)
    {
        return LocationIterator(std::string(location_name));
    }

    template <typename LOCATION_NAME, typename LOCATION_VALUE>
    void RemoveFromLocation(LOCATION_NAME location_name, LOCATION_VALUE location_value)
    {
        LOCK(dimensions_mutex);
        vch_t serialized_location_name = MemoryDataStore::Serialize(location_name);
        if (not dimensions.count(serialized_location_name))
            return;

        MemoryDimension& dimension = dimensions[serialized_location_name];
        vch_t serialized_location_value = MemoryDataStore::Serialize(location_value);
        if (not dimension.located_serialized_objects.count(serialized_location_value))
            return;

        dimension.located_serialized_objects.erase(serialized_location_value);
    }

    template <typename LOCATION_VALUE>
    void RemoveFromLocation(const char* location_name, LOCATION_VALUE location_value)
    {
        RemoveFromLocation(std::string(location_name), location_value);
    }

    template <typename OBJECT_NAME, typename LOCATION_NAME, typename LOCATION_VALUE>
    void GetObjectAtLocation(OBJECT_NAME& object_name, LOCATION_NAME location_name, LOCATION_VALUE location_value)
    {
        LOCK(dimensions_mutex);
        vch_t serialized_location_name = MemoryDataStore::Serialize(location_name);
        if (not dimensions.count(serialized_location_name))
            return;

        MemoryDimension& dimension = dimensions[serialized_location_name];
        vch_t serialized_location_value = MemoryDataStore::Serialize(location_value);
        if (not dimension.located_serialized_objects.count(serialized_location_value))
            return;

        vch_t serialized_object_name = dimension.located_serialized_objects[serialized_location_value];
        Deserialize(serialized_object_name, object_name);
    };

    template <typename OBJECT_NAME, typename LOCATION_VALUE>
    void GetObjectAtLocation(OBJECT_NAME& object_name, const char* location_name, LOCATION_VALUE location_value)
    {
        return GetObjectAtLocation(object_name, std::string(location_name), location_value);
    };

    template <typename OBJECT_NAME>
    void GetObjectAtLocation(OBJECT_NAME& object_name, const char* location_name, const char* location_value)
    {
        return GetObjectAtLocation(object_name, std::string(location_name), std::string(location_value));
    };
};



#endif //TELEPORT_MEMORYDATASTORE_H
