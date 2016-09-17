#ifndef FLEX_MOCKOBJECT_H
#define FLEX_MOCKOBJECT_H

#include "MockProperty.h"
#include "base/serialize.h"
#include "define.h"
#include <map>
#include <string>
#include "MockLocation.h"

#include "log.h"
#define LOG_CATEGORY "MockObject.h"

class MockObject : MockData
{
public:
    vch_t serialized_name;
    std::map<vch_t, MockProperty> properties;
    Mutex properties_mutex;
    std::map<vch_t, MockDimension> *dimensions;
    Mutex dimensions_mutex;
    bool using_internal_dimensions{false};

    MockObject(): using_internal_dimensions(true)
    {
        dimensions = new std::map<vch_t, MockDimension>;
    }

    MockObject(const MockObject& other)
    {
        properties = other.properties;
        serialized_name = other.serialized_name;
        dimensions = new std::map<vch_t, MockDimension>;
        *dimensions = *other.dimensions;
        using_internal_dimensions = other.using_internal_dimensions;
    }

    ~MockObject()
    {
        if (using_internal_dimensions)
            delete dimensions;
    }

    MockObject* operator=(const MockObject& other)
    {
        if (using_internal_dimensions)
            delete dimensions;
        serialized_name = other.serialized_name;
        LOCK(properties_mutex);
        properties = other.properties;
        using_internal_dimensions = other.using_internal_dimensions;
        if (using_internal_dimensions)
        {
            dimensions = new std::map<vch_t, MockDimension>;
            *dimensions = *other.dimensions;
        }
        else
        {
            dimensions = other.dimensions;
        }
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

        LOCK(properties_mutex);
        if (not properties.count(serialized_name))
            properties[serialized_name] = MockProperty();

        return properties[serialized_name];
    }

    MockProperty& operator[] (const char* property_name)
    {
        return (*this)[std::string(property_name)];
    }

    template <typename PROPERTY_NAME>
    bool HasProperty (const PROPERTY_NAME& property_name)
    {
        vch_t serialized_property_name = Serialize(property_name);
        LOCK(properties_mutex);
        return properties.count(serialized_property_name)
               and properties[serialized_property_name].serialized_value.size() > 0;
    }

    bool HasProperty (const char* property_name)
    {
        return HasProperty(std::string(property_name));
    }

    template <typename LOCATION_NAME>
    MockLocation Location(LOCATION_NAME location_name)
    {
        LOCK(dimensions_mutex);
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
