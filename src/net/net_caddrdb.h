#ifndef FLEX_NET_CADDRDB_H
#define FLEX_NET_CADDRDB_H

#include <boost/filesystem/path.hpp>
#include "addrman.h"

/** Access to the (IP) address database (peers.dat) */
class CAddrDB
{
private:
    boost::filesystem::path pathAddr;
public:
    CAddrDB();
    bool Write(const CAddrMan& addr);
    bool Read(CAddrMan& addr);
};

#endif //FLEX_NET_CADDRDB_H
