#ifndef FLEX_MOCKPROPERTY_H
#define FLEX_MOCKPROPERTY_H

#include "MockData.h"
#include <string>
#include <src/base/sync.h>

class MockProperty : MockData
{
public:
    vch_t serialized_value;
    Mutex mutex;

    MockProperty() { }

    MockProperty(const MockProperty& other)
    {
        serialized_value = other.serialized_value;
    }

    MockProperty& operator=(const MockProperty& other)
    {
        serialized_value = other.serialized_value;
        return *this;
    }

    template <typename VALUE> operator VALUE()
    {
        VALUE value;
        LOCK(mutex);
        Deserialize(serialized_value, value);
        return value;
    }

    template <typename VALUE>
    MockProperty& operator=(VALUE value)
    {
        LOCK(mutex);
        serialized_value = Serialize(value);
        return *this;
    }

    MockProperty& operator=(const char* value)
    {
        return (*this) = std::string(value);
    }
};


#endif //FLEX_MOCKPROPERTY_H
