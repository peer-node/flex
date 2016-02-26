// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base/chainparams.h"

#include "assert.h"
#include "base/core.h"
#include "base/protocol.h"
#include "base/util.h"

#include <boost/assign/list_of.hpp>

using namespace boost::assign;

//
// Main network
//

unsigned int pnSeed[] =
{
    //0x12345678
};

class CMainParams : public CChainParams {
public:
    CMainParams() {
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

        // vSeeds.push_back(CDNSSeedData("bitcoin.sipa.be", "seed.bitcoin.sipa.be"));
        // vSeeds.push_back(CDNSSeedData("bluematt.me", "dnsseed.bluematt.me"));
        // vSeeds.push_back(CDNSSeedData("dashjr.org", "dnsseed.bitcoin.dashjr.org"));
        // vSeeds.push_back(CDNSSeedData("bitcoinstats.com", "seed.bitcoinstats.com"));
        // vSeeds.push_back(CDNSSeedData("bitnodes.io", "seed.bitnodes.io"));
        // vSeeds.push_back(CDNSSeedData("xf2.org", "bitseed.xf2.org"));

        base58Prefixes[PUBKEY_ADDRESS] = {0}; //list_of(0);
        base58Prefixes[SCRIPT_ADDRESS] = {5}; //list_of(5);
        base58Prefixes[SECRET_KEY] =     {120}; //list_of(128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E}; //list_of(0x04)(0x88)(0xB2)(0x1E);
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4}; //list_of(0x04)(0x88)(0xAD)(0xE4);

        // Convert the pnSeeds array into usable address objects.
        for (unsigned int i = 0; i < ARRAYLEN(pnSeed); i++)
        {
            // It'll only connect to one or two seed nodes because once it connects,
            // it'll get a pile of addresses with newer timestamps.
            // Seed nodes are given a random 'last seen time' of between one and two
        //     // weeks ago.
        //     const int64_t nOneWeek = 7*24*60*60;
        //     struct in_addr ip;
        //     memcpy(&ip, &pnSeed[i], sizeof(ip));
        //     CAddress addr(CService(ip, GetDefaultPort()));
        //     addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        //     vFixedSeeds.push_back(addr);
        }
    }

    virtual const CBlock& GenesisBlock() const { return genesis; }
    virtual Network NetworkID() const { return CChainParams::MAIN; }

    virtual const vector<CAddress>& FixedSeeds() const {
        return vFixedSeeds;
    }
protected:
    CBlock genesis;
    vector<CAddress> vFixedSeeds;
};
static CMainParams mainParams;


static CChainParams *pCurrentParams = &mainParams;

const CChainParams &Params() {
    return *pCurrentParams;
}
