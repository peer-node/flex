#include "net/net.h"
#include "net/net_cnode.h"

#include "log.h"
#define LOG_CATEGORY "net.cpp"

#if !defined(HAVE_MSG_NOSIGNAL) && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

Network network;
