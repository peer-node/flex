#ifndef FLEX_UTIL_FILE_H
#define FLEX_UTIL_FILE_H

#include <boost/filesystem/path.hpp>

bool RenameOver(boost::filesystem::path src, boost::filesystem::path dest);
bool TryCreateDirectory(const boost::filesystem::path& p);

void FileCommit(FILE *fileout);
bool TruncateFile(FILE *file, unsigned int length);
int RaiseFileDescriptorLimit(int nMinFD);
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length);

boost::filesystem::path GetTempPath();

void ShrinkDebugFile();

#endif //FLEX_UTIL_FILE_H
