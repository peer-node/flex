#include <fstream>
#include <sstream>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <sys/stat.h>
#include "ConfigParser.h"

#include "log.h"
#define LOG_CATEGORY "ConfigParser.cpp"

void ConfigParser::ParseCommandLineArguments(int argc, const char **argv)
{
    config.ParseCommandLineArguments(argc, argv);
}

boost::filesystem::path GetDefaultDataDir()
{
    namespace fs = boost::filesystem;
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\Bitcoin
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\Flex
    // Mac: ~/Library/Application Support/Flex
    // Unix: ~/.flex
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "Flex";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    pathRet /= "Library/Application Support";
    TryCreateDirectory(pathRet);
    return pathRet / "Flex";
#else
    // Unix
    return pathRet / ".flex";
#endif
#endif
}

const boost::filesystem::path ConfigParser::GetDataDir()
{
    boost::filesystem::path path(config.Value("datadir", GetDefaultDataDir().string()));
    return boost::filesystem::system_complete(path);
}

boost::filesystem::path ConfigParser::CompleteConfigFilename(std::string filename)
{
    boost::filesystem::path config_path(filename);
    if (config_path.is_complete())
        return config_path;
    else
        return GetDataDir() / config_path;
}

boost::filesystem::path ConfigParser::ConfigFileName()
{
    std::string filename = config.Value("conf", std::string("flex.conf"));
    return CompleteConfigFilename(filename);
}

bool ConfigParser::HasCorrectPermissions(std::string config_filename)
{
    const char *filename = config_filename.c_str();
    struct stat results;

    stat(filename, &results);

    if (results.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
        return false;
    return (results.st_mode & S_IRUSR) && (results.st_mode & S_IWUSR);
}

bool ConfigParser::Exists(std::string config_filename)
{
    return boost::filesystem::exists(config_filename.c_str());
}

void ConfigParser::ReadConfigFile()
{
    std::string config_filename = ConfigFileName().string();
    CreateDataDirectoryIfItDoesntExist();
    if (not Exists(config_filename))
    {
        throw std::runtime_error("\nThe configuration file " + config_filename + " doesn't exist.\n");
    }
    if (not HasCorrectPermissions(config_filename))
    {
        throw std::runtime_error("\nThe configuration file has insecure permissions.\n"
                                 "Ensure that only your user can read from and write\n"
                                 "to the configuration file. \n"
                                 "E.g. chmod 600 ~/.flex/flex.conf\n");
    }
    std::ifstream config_stream(config_filename);
    std::stringstream buffer;
    buffer << config_stream.rdbuf();
    config.ReadFromStream(buffer);
}

FlexConfig ConfigParser::GetConfig()
{
    return config;
}

void ConfigParser::CreateDataDirectoryIfItDoesntExist()
{
    auto data_directory = GetDataDir().string();
    if (not Exists(data_directory))
    {
        boost::filesystem::path data_dir_path(data_directory);
        boost::filesystem::create_directory(data_dir_path);
    }
}


