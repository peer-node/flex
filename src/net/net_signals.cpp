#include "net.h"
#include "net_signals.h"

// Signals for message handling
CNodeSignals node_signals;
CNodeSignals& GetNodeSignals() { return node_signals; }

