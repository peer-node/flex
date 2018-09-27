#include <src/net/net_cnode.h>
#include "MessageHandlerWithOrphanage.h"
#include "TeleportNetworkNode.h"

#include "log.h"
#define LOG_CATEGORY "MessageHandlerWithOrphanage.cpp"

MessageHandlerWithOrphanage::MessageHandlerWithOrphanage(MemoryDataStore &msgdata):
    msgdata(msgdata)
{ }


std::set<uint160> MessageHandlerWithOrphanage::GetOrphans(uint160 message_hash)
{
    return msgdata[message_hash]["orphans"];
}

bool MessageHandlerWithOrphanage::IsOrphan(uint160 message_hash)
{
    std::set<uint160> missing_dependencies = msgdata[message_hash]["missing_dependencies"];
    if (missing_dependencies.size() != 0)
    {
        log_ << message_hash << " is an orphan\n";
    }
    return missing_dependencies.size() != 0;
}

void MessageHandlerWithOrphanage::HandleOrphan(uint160 message_hash)
{
    if (teleport_network_node != NULL)
        teleport_network_node->HandleMessage(message_hash);
    else
    {
        log_ << "no node attached - can't handle orphans\n";
    }
}
