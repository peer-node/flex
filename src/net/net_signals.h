#ifndef FLEX_NET_SIGNALS_H
#define FLEX_NET_SIGNALS_H

#include "net.h"
#include <boost/signals2/signal.hpp>

// Signals for message handling
struct CNodeSignals
{
    boost::signals2::signal<int ()> GetHeight;
    boost::signals2::signal<bool (CNode*)> ProcessMessages;
    boost::signals2::signal<bool (CNode*, bool)> SendMessages;
    boost::signals2::signal<void (NodeId, const CNode*)> InitializeNode;
    boost::signals2::signal<void (NodeId)> FinalizeNode;
};

extern CNodeSignals node_signals;
CNodeSignals& GetNodeSignals();


#endif //FLEX_NET_SIGNALS_H
