#ifndef FLEX_TESTPEER_H
#define FLEX_TESTPEER_H


class TestPeer : public CNode
{
public:
    std::vector<uint160> dependencies_requested;
    std::vector<uint160> list_expansion_failed_requests;

    Network dummy_network;

    TestPeer(): CNode(dummy_network) { dummy_network.vNodes.push_back(this); }

    virtual void FetchDependencies(std::set<uint160> dependencies)
    {
        dependencies_requested = std::vector<uint160>(dependencies.begin(), dependencies.end());
    }

    virtual void FetchFailedListExpansion(uint160 mined_credit_hash)
    {
        list_expansion_failed_requests.push_back(mined_credit_hash);
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
};


#endif //FLEX_TESTPEER_H
