#include "MainFlexNode.h"


bool MainFlexNode::HaveInventory(const CInv &inv)
{
    return false;
}

bool MainFlexNode::IsFinishedDownloading()
{
    return false;
}

void MainFlexNode::HandleMessage(string command, CDataStream &stream, CNode *peer)
{
    messages_received += 1;
    std::cout << "MainFlexNode received " << command << " plus " << stream.size() << " bytes\n";
}
