#include <src/base/serialize.h>
#include "CreditMessageHandler_.h"
#include "net/net_cnode.h"

void CreditMessageHandler_::HandleMessage(const char *command, CDataStream stream, CNode *peer)
{
    messages_received += 1;
}

void CreditMessageHandler_::SetFlexNode(MainFlexNode &node)
{
    flexnode = &node;
}



