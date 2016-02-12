#ifndef FLEX_LOCATIONITERATOR_H
#define FLEX_LOCATIONITERATOR_H


#include "MockObject.h"
#include "MockDimension.h"

typedef std::map<vch_t, vch_t> MockDataMap;

class LocationIterator : public MockData
{
public:
    MockDataMap::iterator start;
    MockDataMap::iterator end;
    MockDataMap::iterator internal_iterator;

    LocationIterator() { }

    LocationIterator(MockDimension& dimension)
    {
        start = dimension.located_serialized_objects.begin();
        end = dimension.located_serialized_objects.end();
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
};


#endif //FLEX_LOCATIONITERATOR_H
