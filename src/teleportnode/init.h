// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include <string>

namespace boost {
    class thread_group;
};

extern std::string strWalletFile;

void StartShutdown();
bool ShutdownRequested();
void Shutdown();
bool AppInit2(boost::thread_group& threadGroup);

/* The help message mode determines what help message to show */
enum HelpMessageMode
{
    HMM_BITCOIND,
    HMM_BITCOIN_QT
};

std::string HelpMessage(HelpMessageMode mode);

extern std::vector<unsigned char> TCR;

#endif
