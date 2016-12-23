#ifndef TELEPORT_LOCATIONITERATOR_H
#define TELEPORT_LOCATIONITERATOR_H


#include <src/crypto/uint256.h>
#include "MockObject.h"
#include "MockDimension.h"

#include "log.h"
#define LOG_CATEGORY "LocationIterator.h"

typedef std::map<vch_t, vch_t> MockDataMap;

class LocationIterator : public MockData
{
public:
    MockDataMap::iterator start;
    MockDataMap::iterator end;
    MockDataMap::iterator internal_iterator;
    MockDataMap *located_serialized_objects;

    LocationIterator() { }

    LocationIterator(MockDimension& dimension)
    {
        start = dimension.located_serialized_objects.begin();
        end = dimension.located_serialized_objects.end();
        located_serialized_objects = &dimension.located_serialized_objects;
        internal_iterator = start;
    }

    template <typename OBJECT_NAME, typename LOCATION_NAME>
    bool GetNextObjectAndLocation(OBJECT_NAME& object, LOCATION_NAME& location)
    {
        if (internal_iterator == end)
            return false;
        SetObjectAndLocation(object, location);
        internal_iterator++;
        return true;
    }

    template <typename OBJECT_NAME, typename LOCATION_NAME>
    bool GetPreviousObjectAndLocation(OBJECT_NAME& object, LOCATION_NAME& location)
    {
        if (internal_iterator == start)
            return false;
        internal_iterator--;
        SetObjectAndLocation(object, location);
        return true;
    }

    template <typename OBJECT_NAME, typename LOCATION_NAME>
    void SetObjectAndLocation(OBJECT_NAME& object, LOCATION_NAME& location)
    {
        Deserialize(internal_iterator->first, location);
        Deserialize(internal_iterator->second, object);
    };

    void SeekEnd();

    void SeekStart();

    template <typename LOCATION_NAME>
    void Seek(LOCATION_NAME location)
    {
        vch_t serialized_location = Serialize(location);
        internal_iterator = located_serialized_objects->upper_bound(serialized_location);
    }
};


#endif //TELEPORT_LOCATIONITERATOR_H
