#ifndef FLEX_LOCATION_H
#define FLEX_LOCATION_H

#include "MockData.h"


typedef std::map<vch_t, vch_t> MockDataMap;

class MockDimension : public MockData
{
public:
    MockDataMap located_serialized_objects;

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
};


#endif //FLEX_LOCATION_H
