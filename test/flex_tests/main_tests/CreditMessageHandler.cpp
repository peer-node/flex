#include <src/base/serialize.h>
#include "CreditMessageHandler.h"
#include "net/net_cnode.h"

void CreditMessageHandler::HandleMessage(const char *command, CDataStream stream, CNode *peer)
{
    messages_received += 1;
}

void CreditMessageHandler::SetFlexNode(MainFlexNode &node)
{
    flexnode = &node;
}



