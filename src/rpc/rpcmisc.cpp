// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015 The Flex developers
// Distributed under version 3 of the Gnu Affero GPL software license, see the accompanying
// file COPYING

#include "crypto/point.h"
#include "base/base58.h"
#include "flexnode/init.h"
#include "flexnode/main.h"
#include "net/net.h"
#include "net/netbase.h"
#include "rpc/rpcserver.h"
#include "base/util.h"
#include "flexnode/flexnode.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;

Value getinfo(const Array& params, bool fHelp)
{
    Object obj;
    obj.push_back(Pair("latest mined credit hash", 
        flexnode.previous_mined_credit_hash.ToString()));
    MinedCredit mined_credit 
        = creditdata[flexnode.previous_mined_credit_hash]["mined_credit"];
    obj.push_back(Pair("batch number", (uint64_t)mined_credit.batch_number));
    obj.push_back(Pair("offset", (uint64_t)mined_credit.offset));
    RelayState state = flexnode.RelayState();
    obj.push_back(Pair("relays", state.relays.size()));
    obj.push_back(Pair("balance", flexnode.wallet.Balance() * 1e-8));
    return obj;
}

