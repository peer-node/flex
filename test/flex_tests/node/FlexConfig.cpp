#include <boost/program_options/detail/config_file.hpp>
#include <sstream>

#include "FlexConfig.h"


using std::string;
using std::stringstream;
using std::set;
using std::map;
using boost::program_options::detail::config_file_iterator;


uint64_t FlexConfig::size()
{
    return settings.size();
}

string& FlexConfig::operator[](string key)
{
    if (not settings.count(key))
        settings[key] = "";
    return settings[key];
}

void FlexConfig::InterpretNegativeSetting(string key_name)
{
    if (key_name.find("no") == 0)
    {
        std::string name_without_no;
        name_without_no.append(key_name.begin() + 2, key_name.end());

        if (not settings.count(name_without_no))
        {
            settings[name_without_no] = (Value(key_name, false) ? "0" : "1");
        }
    }
}

void FlexConfig::ReadFromStream(stringstream &config_stream)
{
    set<string> options;
    options.insert("*");

    for (config_file_iterator it(config_stream, options), end; it != end; ++it)
    {
        string key = it->string_key, value = it->value[0];

        bool already_set_from_command_line = command_line_settings.count(key) != 0;

        if (not already_set_from_command_line)
        {
            settings[key] = value;
            InterpretNegativeSetting(key);
        }
    }
}

void FlexConfig::ParseCommandLineArgument(string argument)
{
    if (argument[0] != '-')
        return;

    while (argument[0] == '-')
        argument = std::string(argument.begin() + 1, argument.end());

    string key, value;
    size_t equals_location = argument.find("=");

    if (equals_location != string::npos)
    {
        key = argument.substr(0, equals_location);
        value = argument.substr(equals_location + 1);
    }
    else
    {
        key = argument;
        value = "1";
    }

    command_line_settings[key] = value;
    settings[key] = value;
    InterpretNegativeSetting(key);
}

void FlexConfig::ParseCommandLineArguments(int argc, const char **argv)
{
    for (int i = 1; i < argc; i++)
        ParseCommandLineArgument(argv[i]);
}

bool FlexConfig::Value(string key, bool default_value)
{
    if (settings.count(key))
    {
        if (settings[key] == "0")
            return false;
        else if (settings[key] == "1")
            return true;
    }
    return default_value;
}

bool FlexConfig::Bool(string key, bool default_value)
{
    return Value(key, default_value);
}

uint64_t FlexConfig::Value(string key, uint64_t default_value)
{
    if (settings.count(key))
    {
        return std::stoul(settings[key]);
    }
    return default_value;
}

uint64_t FlexConfig::Uint64(string key, uint64_t default_value)
{
    return Value(key, default_value);
}

string FlexConfig::Value(string key, string default_value)
{
    if (settings.count(key))
    {
        return settings[key];
    }
    return default_value;
}

string FlexConfig::String(string key, string default_value)
{
    return Value(key, default_value);
}

