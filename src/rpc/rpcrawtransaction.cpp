// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/point.h"
#include "base/base58.h"
#include "base/core.h"
#include "flexnode/init.h"
#include "database/keystore.h"
#include "flexnode/main.h"
#include "net/net.h"
#include "rpc/rpcserver.h"
#include "crypto/uint256.h"


#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;
