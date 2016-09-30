#ifndef TELEPORT_TESTPEERWITHNETWORKNODE_H
#define TELEPORT_TESTPEERWITHNETWORKNODE_H

#include "TestPeer.h"
#include "TeleportNetworkNode.h"

#include "log.h"
#define LOG_CATEGORY "TestPeerWithNetworkNode.h"

class TestPeerWithNetworkNode : public TestPeer
{
public:
    TeleportNetworkNode* teleport_network_node{NULL};
    std::vector<CInv> invs_processed;
    bool getting_messages{true};
    boost::thread *thread{NULL};

    TestPeerWithNetworkNode()
    {
        thread = new boost::thread(&TestPeerWithNetworkNode::GetMessages, this);
    }

    ~TestPeerWithNetworkNode()
    {
        delete thread;
    }

    void StopGettingMessages()
    {   // this must be called before the peer destructs
        getting_messages = false;
        thread->join();
    }

    void GetMessages()
    {
        std::vector<CInv> inventory;
        while (getting_messages)
        {
            boost::this_thread::sleep(boost::posix_time::milliseconds(20));
            boost::this_thread::interruption_point();
            if (not getting_messages)
                return;
            {
                inventory = vInventoryToSend;
            }
            for (auto inv : inventory)
            {
                GetMessageIfItHasNotAlreadyBeenProcessed(inv);
            }
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
        if (not getting_messages or teleport_network_node == NULL)
            return;
        teleport_network_node->HandleMessage(channel, ssReceived, ConnectedPeerIfThereIsOne());
        invs_processed.push_back(inv);
    }

    CDataStream GetDataStreamForInv(CInv inv)
    {
        LOCK(network.cs_vNodes);
        for (uint32_t tries = 0; tries < 5; tries++)
        {
            for (auto peer : network.vNodes)
            {
                LOCK(peer->network.cs_mapRelay);
                if (peer->network.mapRelay.count(inv))
                {
                    return (*peer->network.mapRelay.find(inv)).second;
                }
            }
            MilliSleep(5);
        }
        log_ << "Couldn't find datastream for inv " << inv.hash << "\n";
        return CDataStream(SER_NETWORK, CLIENT_VERSION);
    }

    void SetNetworkNode(TeleportNetworkNode *teleport_network_node_)
    {
        teleport_network_node = teleport_network_node_;
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

        ssSend.clear();

        if (teleport_network_node != NULL)
            teleport_network_node->HandleMessage(channel, ssReceived, ConnectedPeerIfThereIsOne());

        LEAVE_CRITICAL_SECTION(cs_vSend);
    }

    CNode *ConnectedPeerIfThereIsOne()
    {
        if (network.vNodes.size() == 2)
            return this == network.vNodes[0] ? network.vNodes[1] : network.vNodes[0];

        return NULL;
    }
};

#endif //TELEPORT_TESTPEERWITHNETWORKNODE_H
