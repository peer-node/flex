#ifndef TELEPORT_MEMORYDIMENSION_H
#define TELEPORT_MEMORYDIMENSION_H

#include <src/base/sync.h>
#include "MemoryData.h"


typedef std::map<vch_t, vch_t> MockDataMap;

class MemoryDimension : public MemoryData
{
public:
    MockDataMap located_serialized_objects;
    Mutex mutex;

    MemoryDimension() { }

    MemoryDimension(const MemoryDimension& other)
    {
        located_serialized_objects = other.located_serialized_objects;
    }

    MemoryDimension& operator=(const MemoryDimension& other)
    {
        located_serialized_objects = other.located_serialized_objects;
        return *this;
    }

    bool Find(vch_t serialized_object_name, vch_t& location_value)
    {
        MockDataMap::iterator finder = located_serialized_objects.begin();
        for (; finder != located_serialized_objects.end(); finder++)
            if (finder->second == serialized_object_name)
            {
                location_value = finder->first;
                return true;
            }
        return false;
    }

    void Delete(vch_t serialized_object_name)
    {
        vch_t serialized_location_value;
        if (not Find(serialized_object_name, serialized_location_value))
            return;
        located_serialized_objects.erase(serialized_location_value);
    }
};


#endif //TELEPORT_MEMORYDIMENSION_H
