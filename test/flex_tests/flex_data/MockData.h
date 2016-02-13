#ifndef FLEX_MOCKDATA_H
#define FLEX_MOCKDATA_H

#include "base/serialize.h"
#include "base/version.h"
#include "define.h"




class MockData
{
public:
    template <typename N>
    vch_t Serialize(const N &object_name) const
    {
        CDataStream stream(SER_DISK, CLIENT_VERSION);
        stream << object_name;
        vch_t serialized = ReverseBytes(vch_t(stream.begin(), stream.end()));
        return serialized;
    }

    template <typename VALUE>
    void Deserialize(vch_t serialized_data, VALUE& value)
    {
        VALUE blank;
        value = blank;

        serialized_data = ReverseBytes(serialized_data);
        CDataStream stream(serialized_data,
                           SER_DISK,
                           CLIENT_VERSION);
        try
        {
            stream >> value;
        } catch (...) { }
    }

private:
    vch_t ReverseBytes(vch_t input) const
    {
        std::reverse(input.begin(), input.end());
        return input;
    }
};


#endif //FLEX_MOCKDATA_H
