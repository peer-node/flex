#ifndef TELEPORT_CONFIGPARSER_H
#define TELEPORT_CONFIGPARSER_H

#include <boost/filesystem/path.hpp>

#include "TeleportConfig.h"

class ConfigParser
{
public:
    TeleportConfig config;

    ConfigParser() { }

    void ParseCommandLineArguments(int argc, const char *argv[]);

    void ReadConfigFile();

    boost::filesystem::path ConfigFileName();

    boost::filesystem::path CompleteConfigFilename(std::string filename);

    const boost::filesystem::path GetDataDir();

    bool HasCorrectPermissions(std::string config_filename);

    TeleportConfig GetConfig();

    bool Exists(std::string config_filename);

    void CreateDataDirectoryIfItDoesntExist();
};


#endif //TELEPORT_CONFIGPARSER_H
