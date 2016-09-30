#ifndef TELEPORT_NET_CADDRDB_H
#define TELEPORT_NET_CADDRDB_H

#include <boost/filesystem/path.hpp>
#include "addrman.h"

/** Access to the (IP) address database (peers.dat) */
class CAddrDB
{
private:
    boost::filesystem::path pathAddr;
    std::string network_name;
public:
    CAddrDB(std::string network_name);
    bool Write(const CAddrMan& addr);
    bool Read(CAddrMan& addr);
};

#endif //TELEPORT_NET_CADDRDB_H
