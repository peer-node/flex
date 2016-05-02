// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base/chainparams.h"

#include "base/protocol.h"
#include "util_hex.h"
#include "util_time.h"
#include "util_rand.h"

#include <boost/assign/list_of.hpp>

using namespace boost::assign;
using namespace std;

//
// Main network
//

unsigned int pnSeed[] =
{
    //0x12345678
};

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0xe9;
        pchMessageStart[1] = 0xec;
        pchMessageStart[2] = 0xe4;
        pchMessageStart[3] = 0xe9;
        vAlertPubKey = ParseHex("");
        nDefaultPort = 8733;
        nRPCPort = 8732;

        // vSeeds.push_back(CDNSSeedData("domain.com", "subdomain.domain.com"));

        base58Prefixes[PUBKEY_ADDRESS] = {0};
        base58Prefixes[SCRIPT_ADDRESS] = {5};
        base58Prefixes[SECRET_KEY] =     {120};
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        // Convert the pnSeeds array into usable address objects.
        for (unsigned int i = 0; i < ARRAYLEN(pnSeed); i++)
        {
            // It'll only connect to one or two seed nodes because once it connects,
            // it'll get a pile of addresses with newer timestamps.
            // Seed nodes are given a random 'last seen time' of between one and two
            // weeks ago.
             const int64_t nOneWeek = 7*24*60*60;
             struct in_addr ip;
             memcpy(&ip, &pnSeed[i], sizeof(ip));
             CAddress addr(CService(ip, (unsigned short)GetDefaultPort()));
             addr.nTime = (uint32_t) (GetTime() - GetRand(nOneWeek) - nOneWeek);
             vFixedSeeds.push_back(addr);
        }
    }

    virtual Network NetworkID() const { return CChainParams::MAIN; }

    virtual const vector<CAddress>& FixedSeeds() const {
        return vFixedSeeds;
    }
protected:
    vector<CAddress> vFixedSeeds;
};
static CMainParams mainParams;


static CChainParams *pCurrentParams = &mainParams;

const CChainParams &Params() {
    return *pCurrentParams;
}
