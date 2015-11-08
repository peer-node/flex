#ifndef FLEX_LOG
#define FLEX_LOG

#include "util.h"
#include "define.h"

#ifdef LOG_CATEGORY
#undef LOG_CATEGORY
#endif
#define LOG_CATEGORY "unspecified"


class Logger
{
public:
    Logger& operator<<(uint64_t n)
    {
        char buffer[BUFSIZ];
        sprintf(buffer, "%lu", n);
        return *this << std::string(buffer);
    }

    Logger& operator<<(int64_t n)
    {
        char buffer[BUFSIZ];
        sprintf(buffer, "%ld", n);
        return *this << std::string(buffer);
    }

    Logger& operator<<(double n)
    {
        char buffer[BUFSIZ];
        sprintf(buffer, "%lf", n);
        return *this << std::string(buffer);
    }

    Logger& operator<<(uint32_t n)
    {
        char buffer[BUFSIZ];
        sprintf(buffer, "%u", n);
        return *this << std::string(buffer);
    }

    Logger& operator<<(int32_t n)
    {
        char buffer[BUFSIZ];
        sprintf(buffer, "%d", n);
        return *this << std::string(buffer);
    }

    Logger& operator<<(uint8_t n)
    {
        char buffer[BUFSIZ];
        sprintf(buffer, "%02x", n);
        return *this << std::string(buffer);
    }

    Logger& operator<<(char n)
    {
        char buffer[BUFSIZ];
        sprintf(buffer, "%02x", (uint8_t)n);
        return *this << std::string(buffer);
    }

    Logger& operator<<(vch_t bytes)
    {
        for (uint32_t i = 0; i < bytes.size(); i++)
            *this << bytes[i];
        return *this;
    }

    Logger& operator<<(bool b)
    {
        return *this << std::string(b ? "true" : "false");
    }

    template <typename V>
    Logger& operator<<(V value)
    {
        return *this << value.ToString();
    }

    template <typename V>
    Logger& operator<<(std::vector<V> values)
    {
        *this << "(";
        for (uint32_t i = 0; i < values.size(); i++)
            *this << values[i] << ",";
        *this << ")";
        return *this;
    }

    template <typename V, typename W>
    Logger& operator<<(std::pair<V, W> value)
    {
        *this << "(";
        *this << value.first;
        *this << ",";
        *this << value.second;
        *this << ")";
        return *this;
    }

    Logger& operator<<(std::string value)
    {
        return *this << value.c_str();
    }

    Logger& operator<<(const char* value)
    {
        LogPrint(LOG_CATEGORY, value);
        return *this;
    }
};


#define log_ Logger() << LOG_CATEGORY << ": "

#endif

#undef LOG_CATEGORY