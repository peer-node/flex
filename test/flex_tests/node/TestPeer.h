#ifndef FLEX_TESTPEER_H
#define FLEX_TESTPEER_H

#include "net/net_cnode.h"

#include "log.h"
#define LOG_CATEGORY "TestPeer.h"

class TestPeer : public CNode
{
public:
    std::vector<uint160> dependencies_requested;
    std::vector<std::string> pushed_messages;

    Network dummy_network;

    TestPeer(): CNode(dummy_network)
    {
        hSocket = INVALID_SOCKET;
        dummy_network.vNodes.push_back(this);
    }

    virtual void FetchDependencies(std::set<uint160> dependencies)
    {
        dependencies_requested = std::vector<uint160>(dependencies.begin(), dependencies.end());
    }

    bool HasBeenInformedAbout(CDataStream& ss)
    {
        uint256 hash = Hash(ss.begin(), ss.end());
        CInv inv(MSG_GENERAL, hash);
        return VectorContainsEntry(vInventoryToSend, inv);
    }

    template<typename T1>
    bool HasBeenInformedAbout(const char* command, const char* subcommand, const T1& message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << std::string(command) << std::string(subcommand) << message;
        return HasBeenInformedAbout(ss);
    }

    bool HasReceived(const char* command, const char* subcommand)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << std::string(command) << std::string(subcommand);
        return VectorContainsEntry(pushed_messages, ss.str());
    }

    template<typename T1>
    bool HasReceived(const char* command, const char* subcommand, const T1& message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << CMessageHeader(command, 0) << std::string(subcommand) << message;
        return VectorContainsEntry(pushed_messages, ss.str());
    }

    void EndMessage() UNLOCK_FUNCTION(cs_vSend)
    {
        // record message instead of actually sending it
        pushed_messages.push_back(ssSend.str());
        ssSend.clear();
        LEAVE_CRITICAL_SECTION(cs_vSend);
    }

};


#endif //FLEX_TESTPEER_H
