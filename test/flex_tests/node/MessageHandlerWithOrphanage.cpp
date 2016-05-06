#include <src/net/net_cnode.h>
#include "MessageHandlerWithOrphanage.h"

MessageHandlerWithOrphanage::MessageHandlerWithOrphanage(MemoryDataStore &msgdata):
    msgdata(msgdata)
{

}

std::set<uint160> MessageHandlerWithOrphanage::GetOrphans(uint160 message_hash)
{
    return msgdata[message_hash]["orphans"];
}

bool MessageHandlerWithOrphanage::IsOrphan(uint160 message_hash)
{
    std::set<uint160> missing_dependencies = msgdata[message_hash]["missing_dependencies"];
    return missing_dependencies.size() != 0;
}
