#ifndef FLEX_TESTPEERWITHNETWORKNODE_H
#define FLEX_TESTPEERWITHNETWORKNODE_H

#include "TestPeer.h"
#include "FlexNetworkNode.h"


class TestPeerWithNetworkNode : public TestPeer
{
public:
    FlexNetworkNode* flex_network_node{NULL};

    void SetNetworkNode(FlexNetworkNode *flex_network_node_)
    {
        flex_network_node = flex_network_node_;
    }

    void EndMessage() UNLOCK_FUNCTION(cs_vSend)
    {
        pushed_messages.push_back(ssSend.str());
        std::string channel;
        ssSend >> channel;
        if (flex_network_node != NULL)
            flex_network_node->HandleMessage(channel, ssSend, this);
        ssSend.clear();
        LEAVE_CRITICAL_SECTION(cs_vSend);
    }

};

#endif //FLEX_TESTPEERWITHNETWORKNODE_H
