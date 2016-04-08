#include "base/util.h"
#include "ui_interface.h"
#include "sync.h"
#include <set>
#include <string>
#include <boost/filesystem/operations.hpp>

using namespace std;


boost::filesystem::path GetPidFile()
{
    boost::filesystem::path pathPidFile(GetArg("-pid", "flexd.pid"));
    if (!pathPidFile.is_complete()) pathPidFile = GetDataDir_() / pathPidFile;
    return pathPidFile;
}

#ifndef WIN32
void CreatePidFile(const boost::filesystem::path &path, pid_t pid)
{
    FILE* file = fopen(path.string().c_str(), "w");
    if (file)
    {
        fprintf(file, "%d\n", pid);
        fclose(file);
    }
}
#endif
