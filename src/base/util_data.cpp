#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include "base/util_args.h"
#include "base/util_data.h"
#include "sync.h"
#include "chainparams.h"

using namespace std;

static boost::filesystem::path pathCached[CChainParams::MAX_NETWORK_TYPES+1];
static CCriticalSection csPathCached;

boost::filesystem::path GetDefaultDataDir_()
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


const boost::filesystem::path GetDataDir_(bool fNetSpecific)
{
    namespace fs = boost::filesystem;

    LOCK(csPathCached);

    fs::path path;

    if (mapArgs.count("-datadir")) {
        path = fs::system_complete(mapArgs["-datadir"]);
        if (!fs::is_directory(path)) {
            path = GetDefaultDataDir_();
            return path;
        }
    } else {
        path = GetDefaultDataDir_();
    }

    fs::create_directories(path);

    return path;
}

void ClearDatadirCache()
{
    std::fill(&pathCached[0], &pathCached[CChainParams::MAX_NETWORK_TYPES+1],
              boost::filesystem::path());
}

boost::filesystem::path GetConfigFile()
{
    boost::filesystem::path pathConfigFile(GetArg("-conf", "flex.conf"));
    if (!pathConfigFile.is_complete()) pathConfigFile = GetDataDir_(false) / pathConfigFile;
    return pathConfigFile;
}

bool CheckConfigFilePermissions(const char *filename)
{
    struct stat results;

    stat(filename, &results);

    if (results.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
        return false;
    return (results.st_mode & S_IRUSR) && (results.st_mode & S_IWUSR);
}

void ReadConfigFile(map<string, string>& mapSettingsRet,
                    map<string, vector<string> >& mapMultiSettingsRet)
{
    std::stringstream config_stream;

    if (mapArgs.count("-conf") && mapArgs["-conf"] == "-")
    {
        config_stream << std::cin.rdbuf();
    }
    else
    {
        boost::filesystem::ifstream streamConfig(GetConfigFile());
        if (!streamConfig.good())
            return; // No flex.conf file is OK

        if (!CheckConfigFilePermissions(GetConfigFile().string().c_str()))
            throw runtime_error(
                "\nThe configuration file has insecure permissions.\n"
                "Ensure that only your user can read from and write\n"
                "to the configuration file. \n"
                "E.g. chmod 600 ~/.flex/flex.conf\n"
                );
        config_stream << streamConfig.rdbuf();
    }

    set<string> setOptions;
    setOptions.insert("*");

    for (boost::program_options::detail::config_file_iterator
         it(config_stream, setOptions), end; it != end; ++it)
    {
        // Don't overwrite existing settings so command line settings override flex.conf
        string strKey = string("-") + it->string_key;
        if (mapSettingsRet.count(strKey) == 0)
        {
            mapSettingsRet[strKey] = it->value[0];
            // interpret nofoo=1 as foo=0 (and nofoo=0 as foo=1) as long as foo not set)
            InterpretNegativeSetting(strKey, mapSettingsRet);
        }
        mapMultiSettingsRet[strKey].push_back(it->value[0]);
    }
    // If datadir is changed in .conf file:
    ClearDatadirCache();
}
