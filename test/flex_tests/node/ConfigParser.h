#ifndef FLEX_CONFIGPARSER_H
#define FLEX_CONFIGPARSER_H

#include <boost/filesystem/path.hpp>

#include "FlexConfig.h"

class ConfigParser
{
public:
    FlexConfig config;

    ConfigParser() { }

    void ParseCommandLineArguments(int argc, const char *argv[]);

    void ReadConfigFile();

    boost::filesystem::path ConfigFileName();

    boost::filesystem::path CompleteConfigFilename(std::string filename);

    const boost::filesystem::path GetDataDir();

    bool HasCorrectPermissions(std::string config_filename);

    FlexConfig GetConfig();

    bool Exists(std::string config_filename);

    void CreateDataDirectoryIfItDoesntExist();
};


#endif //FLEX_CONFIGPARSER_H
