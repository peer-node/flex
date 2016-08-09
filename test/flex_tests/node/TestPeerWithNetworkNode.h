#ifndef FLEX_TESTPEERWITHNETWORKNODE_H
#define FLEX_TESTPEERWITHNETWORKNODE_H

#include "TestPeer.h"
#include "FlexNetworkNode.h"

#include "log.h"
#define LOG_CATEGORY "TestPeerWithNetworkNode.h"

class TestPeerWithNetworkNode : public TestPeer
{
public:
    FlexNetworkNode* flex_network_node{NULL};
    std::vector<CInv> invs_processed;
    bool getting_messages{true};
    boost::thread *thread{NULL};

    TestPeerWithNetworkNode()
    {
        boost::thread t(&TestPeerWithNetworkNode::GetMessages, this);
        thread = &t;
    }

    ~TestPeerWithNetworkNode()
    {

    }

    void StopGettingMessages()
    {
        getting_messages = false;
    }

    void GetMessages()
    {
        while (getting_messages)
        {
            for (auto inv : vInventoryToSend)
                GetMessageIfItHasNotAlreadyBeenProcessed(inv);

            boost::this_thread::interruption_point();
            if (not getting_messages)
                return;
            boost::this_thread::sleep(boost::posix_time::milliseconds(20));
        }
    }

    void GetMessageIfItHasNotAlreadyBeenProcessed(CInv inv)
    {
        if (not getting_messages)
            return;
        if (VectorContainsEntry(invs_processed, inv))
            return;

        auto ss = GetDataStreamForInv(inv);
        CDataStream ssReceived(SER_NETWORK, CLIENT_VERSION);
        ssReceived += ss;
        std::string channel;
        ss >> channel;
        if (not getting_messages or flex_network_node == NULL)
            return;
        flex_network_node->HandleMessage(channel, ssReceived, ConnectedPeerIfThereIsOne());
        invs_processed.push_back(inv);
    }

    CDataStream GetDataStreamForInv(CInv inv)
    {
        for (auto peer : network.vNodes)
        {
            if (peer->network.mapRelay.count(inv))
                return (*peer->network.mapRelay.find(inv)).second;
        }
        log_ << "Coudn't find datastream for inv " << inv.hash << "\n";
        return CDataStream(SER_NETWORK, CLIENT_VERSION);
    }

    void SetNetworkNode(FlexNetworkNode *flex_network_node_)
    {
        flex_network_node = flex_network_node_;
    }

    void EndMessage() UNLOCK_FUNCTION(cs_vSend)
    {
        pushed_messages.push_back(ssSend.str());

        CDataStream ssReceived(SER_NETWORK, CLIENT_VERSION);
        CMessageHeader header;
        ssSend >> header;
        std::string channel(header.pchCommand);

        ssReceived << channel;
        ssReceived = ssReceived + ssSend;

        if (flex_network_node != NULL)
            flex_network_node->HandleMessage(channel, ssReceived, ConnectedPeerIfThereIsOne());

        ssSend.clear();
        LEAVE_CRITICAL_SECTION(cs_vSend);
    }

    CNode *ConnectedPeerIfThereIsOne()
    {
        if (network.vNodes.size() == 2)
            return this == network.vNodes[0] ? network.vNodes[1] : network.vNodes[0];

        return NULL;
    }
};

#endif //FLEX_TESTPEERWITHNETWORKNODE_H
