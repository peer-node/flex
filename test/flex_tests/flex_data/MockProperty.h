#ifndef FLEX_MOCKPROPERTY_H
#define FLEX_MOCKPROPERTY_H

#include "MockData.h"
#include <string>

class MockProperty : MockData
{
public:
    vch_t serialized_value;

    template <typename VALUE> operator VALUE()
    {
        VALUE value;
        Deserialize(serialized_value, value);
        return value;
    }

    template <typename VALUE>
    MockProperty& operator=(VALUE value)
    {
        serialized_value = Serialize(value);
        return *this;
    }

    MockProperty& operator=(const char* value)
    {
        return (*this) = std::string(value);
    }
};


#endif //FLEX_MOCKPROPERTY_H
