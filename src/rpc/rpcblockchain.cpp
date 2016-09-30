// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/point.h"
#include "rpc/rpcserver.h"
#include "teleportnode/main.h"
#include "base/sync.h"
#include "teleportnode/teleportnode.h"

#include <stdint.h>

#include "json/json_spirit_value.h"

using namespace json_spirit;
using namespace std;


Value getdifficulty(const Array& params, bool fHelp)
{
    return teleportnode.pit.mined_credit.difficulty.ToString();
}

