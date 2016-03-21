#ifndef FLEX_FLEXCONFIG_H
#define FLEX_FLEXCONFIG_H


#include <string>
#include <map>

class FlexConfig
{
public:
    unsigned long size();
    std::string& operator[](std::string key);

    std::map<std::string, std::string> settings;

    void ReadFromStream(std::stringstream &config_stream);

    void ParseCommandLineArguments(int argc, const char *argv[]);

    void ParseCommandLineArgument(std::string argument);

    void InterpretNegativeSetting(std::string key_name);

    bool Value(std::string key, bool default_value);

    uint64_t Value(std::string key, uint64_t default_value);

    std::string Value(std::string key, std::string default_value);
};


#endif //FLEX_FLEXCONFIG_H
