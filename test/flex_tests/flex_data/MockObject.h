#ifndef FLEX_MOCKOBJECT_H
#define FLEX_MOCKOBJECT_H

#include "MockProperty.h"
#include "base/serialize.h"
#include "define.h"
#include <map>
#include <string>
#include "MockLocation.h"


class MockObject : MockData
{
public:
    vch_t serialized_name;
    std::map<vch_t, MockProperty> properties;
    std::map<vch_t, MockDimension> *dimensions;
    bool using_internal_dimensions;

    MockObject(): using_internal_dimensions(true)
    {
        dimensions = new std::map<vch_t, MockDimension>;
    }

    ~MockObject()
    {
        if (using_internal_dimensions)
            delete dimensions;
    }

    MockObject(vch_t serialized_name, std::map<vch_t, MockDimension> *dimensions):
        serialized_name(serialized_name),
        dimensions(dimensions),
        using_internal_dimensions(false)
    {

    }

    template <typename PROPERTY_NAME>
    MockProperty& operator[] (const PROPERTY_NAME& property_name)
    {
        vch_t serialized_name = Serialize(property_name);

        if (not properties.count(serialized_name))
            properties[serialized_name] = MockProperty();

        return properties[serialized_name];
    }

    MockProperty& operator[] (const char* property_name)
    {
        return (*this)[std::string(property_name)];
    }

    template <typename LOCATION_NAME>
    MockLocation Location(LOCATION_NAME location_name)
    {
        vch_t serialized_location_name = Serialize(location_name);

        if (not dimensions->count(serialized_location_name))
            (*dimensions)[serialized_location_name] = MockDimension();

        MockDimension& dimension = (*dimensions)[serialized_location_name];
        vch_t serialized_location_value;

        if (not dimension.Find(serialized_name, serialized_location_value))
            serialized_location_value = vch_t();

        return MockLocation(serialized_name, dimension, serialized_location_value);
    }

    MockLocation Location(const char* location_name)
    {
        return Location(std::string(location_name));
    }
};


#endif //FLEX_MOCKOBJECT_H
