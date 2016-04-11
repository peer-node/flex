// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#if defined(HAVE_CONFIG_H)
#include "flex-config.h"
#endif

#include "compat.h"
#include "base/serialize.h"
#include "base/tinyformat.h"

#include <cstdio>
#include <exception>
#include <map>
#include <stdarg.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <boost/filesystem/path.hpp>
#include <boost/thread.hpp>

static const int64_t COIN = 100000000;
static const int64_t CENT = 1000000;

#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))
#define UBEGIN(a)           ((unsigned char*)&(a))
#define UEND(a)             ((unsigned char*)&((&(a))[1]))
#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define PAIRTYPE(t1, t2)    std::pair<t1, t2>


#ifdef WIN32
#define MSG_DONTWAIT        0

#ifndef S_IRUSR
#define S_IRUSR             0400
#define S_IWUSR             0200
#define S_IRGRP             0040
#define S_IWGRP             0020
#define S_IROTH             0004
#define S_IWOTH             0002
#endif
#else
#define MAX_PATH            1024
#endif
// As Solaris does not have the MSG_NOSIGNAL flag for send(2) syscall, it is defined as 0
#if !defined(HAVE_MSG_NOSIGNAL) && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif


#endif
