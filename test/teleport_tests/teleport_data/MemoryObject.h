#ifndef TELEPORT_MEMORYOBJECT_H
#define TELEPORT_MEMORYOBJECT_H

#include "MemoryProperty.h"
#include "base/serialize.h"
#include "define.h"
#include <map>
#include <string>
#include "MemoryLocation.h"

#include "log.h"

#define LOG_CATEGORY "MockObject.h"

class MemoryObject : MemoryData
{
public:
    vch_t serialized_name;
    std::map<vch_t, MemoryProperty> properties;
    Mutex properties_mutex;
    std::map<vch_t, MemoryDimension> *dimensions;
    Mutex dimensions_mutex;
    bool using_internal_dimensions{false};

    MemoryObject(): using_internal_dimensions(true)
    {
        dimensions = new std::map<vch_t, MemoryDimension>;
    }

    MemoryObject(const MemoryObject& other)
    {
        properties = other.properties;
        serialized_name = other.serialized_name;
        dimensions = new std::map<vch_t, MemoryDimension>;
        *dimensions = *other.dimensions;
        using_internal_dimensions = other.using_internal_dimensions;
    }

    ~MemoryObject()
    {
        if (using_internal_dimensions)
            delete dimensions;
    }

    MemoryObject* operator=(const MemoryObject& other)
    {
        if (using_internal_dimensions)
            delete dimensions;
        serialized_name = other.serialized_name;
        LOCK(properties_mutex);
        properties = other.properties;
        using_internal_dimensions = other.using_internal_dimensions;
        if (using_internal_dimensions)
        {
            dimensions = new std::map<vch_t, MemoryDimension>;
            *dimensions = *other.dimensions;
        }
        else
        {
            dimensions = other.dimensions;
        }
    }

    MemoryObject(vch_t serialized_name, std::map<vch_t, MemoryDimension> *dimensions):
        serialized_name(serialized_name),
        dimensions(dimensions),
        using_internal_dimensions(false)
    {

    }

    template <typename PROPERTY_NAME>
    MemoryProperty& operator[] (const PROPERTY_NAME& property_name)
    {
        vch_t serialized_name = Serialize(property_name);

        LOCK(properties_mutex);
        if (not properties.count(serialized_name))
            properties[serialized_name] = MemoryProperty();

        return properties[serialized_name];
    }

    MemoryProperty& operator[] (const char* property_name)
    {
        return (*this)[std::string(property_name)];
    }

    template <typename PROPERTY_NAME>
    bool HasProperty(const PROPERTY_NAME& property_name)
    {
        vch_t serialized_property_name = Serialize(property_name);
        LOCK(properties_mutex);
        return properties.count(serialized_property_name)
               and properties[serialized_property_name].serialized_value.size() > 0;
    }

    bool HasProperty(const char* property_name)
    {
        return HasProperty(std::string(property_name));
    }

    template <typename LOCATION_NAME>
    MemoryLocation Location(LOCATION_NAME location_name)
    {
        LOCK(dimensions_mutex);
        vch_t serialized_location_name = Serialize(location_name);

        if (not dimensions->count(serialized_location_name))
            (*dimensions)[serialized_location_name] = MemoryDimension();

        MemoryDimension& dimension = (*dimensions)[serialized_location_name];
        vch_t serialized_location_value;

        if (not dimension.Find(serialized_name, serialized_location_value))
            serialized_location_value = vch_t();

        return MemoryLocation(serialized_name, dimension, serialized_location_value);
    }

    MemoryLocation Location(const char* location_name)
    {
        return Location(std::string(location_name));
    }
};


#endif //TELEPORT_MEMORYOBJECT_H
