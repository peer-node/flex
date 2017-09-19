#ifndef TELEPORT_STRING_FUNCTIONS_H
#define TELEPORT_STRING_FUNCTIONS_H

#include <boost/algorithm/string.hpp>
#include <map>
#include <sstream>

typedef std::map<std::string, std::string> StringMap;



inline bool Contains(std::string text, std::string target)
{
    size_t location = text.find(target);
    return location != std::string::npos;
}

inline std::string Replace(std::string text, std::string target, std::string replacement)
{
    size_t location;
    location = text.find(target);
    while (location != std::string::npos)
    {
        text = text.replace(location, target.size(), replacement);
        location = text.find(target);
    }
    return text;
}

inline int64_t string_to_int(std::string str_num)
{
    return (int64_t) strtoll(str_num.c_str(), NULL, 0);
}

inline uint64_t string_to_uint(std::string str_num)
{
    return strtoul(str_num.c_str(), NULL, 0);
}

template <typename T> std::string ToString(T value)
{
    std::stringstream ss;
    ss << value;
    return ss.str();
}

inline std::string lowercase(std::string s)
{
    boost::algorithm::to_lower(s);
    return s;
}

inline bool split_string(std::string whole, std::string& part1, std::string& part2, const char *delimiter=":")
{
    size_t delimiter_location = whole.find(delimiter);
    if (delimiter_location == std::string::npos)
        return false;
    part1 = whole.substr(0, delimiter_location);
    part2 = whole.substr(delimiter_location + 1,
                         whole.size() - delimiter_location - 1);
    return true;
}

inline bool GetMethodAndParamsFromCommand(std::string& method, std::vector<std::string>& params, std::string command)
{
    std::string part, remaining_part;
    if (command.find(" ") == std::string::npos)
    {
        method = command;
        return true;
    }
    if (!split_string(command, method, remaining_part, " "))
        return false;

    while (split_string(remaining_part, part, remaining_part, " "))
    {
        params.push_back(part);
    }
    params.push_back(remaining_part);
    return true;
}

inline std::string GetStringBetweenQuotes(std::string text)
{
    if (Contains(text, "\""))
    {
        std::string preceding_characters, target, succeeding_characters;
        split_string(text, preceding_characters, target, "\"");
        split_string(target, text, succeeding_characters, "\"");
    }
    return text;
}

inline std::string AmountToString(uint64_t amount, uint32_t decimal_places)
{
    char char_amount[50];
    if (decimal_places == 0)
    {
        sprintf(char_amount, "%lu", amount);
        return std::string(char_amount);
    }

    std::string string_amount = AmountToString(amount, 0);
    while (string_amount.size() < decimal_places)
        string_amount = "0" + string_amount;

    auto l = string_amount.size();
    string_amount = string_amount.substr(0, l - decimal_places)
                    + "." +
                    string_amount.substr(l - decimal_places, decimal_places);

    if (string_amount.substr(0, 1) == ".")
        string_amount = "0" + string_amount;

    return string_amount;
}

#endif //TELEPORT_STRING_FUNCTIONS_H
