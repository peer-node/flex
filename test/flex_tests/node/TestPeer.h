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
};


#endif //FLEX_TESTPEER_H
