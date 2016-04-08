#ifndef FLEX_UTIL_DATA_H
#define FLEX_UTIL_DATA_H

boost::filesystem::path GetDefaultDataDir_();
const boost::filesystem::path GetDataDir_(bool fNetSpecific = true);
boost::filesystem::path GetConfigFile();
boost::filesystem::path GetPidFile();
#ifndef WIN32
void CreatePidFile(const boost::filesystem::path &path, pid_t pid);
#endif
void ReadConfigFile(std::map<std::string, std::string>& mapSettingsRet, std::map<std::string, std::vector<std::string> >& mapMultiSettingsRet);
#ifdef WIN32
boost::filesystem::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif


#endif //FLEX_UTIL_DATA_H
