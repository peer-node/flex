#include "net.h"
#include "net_cnode.h"

#include "log.h"
#define LOG_CATEGORY "net_relay.cpp"

void Network::TellNodeAboutTransaction(CNode* pnode, const SignedTransaction& tx)
{
    uint256 hash = tx.GetHash();
    CInv inv(MSG_TX, hash);
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << tx;

    mapRelay.insert(std::make_pair(inv, ss));
    LOCK(cs_vNodes);
    {
        pnode->PushInventory(inv);
    }
}

void Network::ExpireOldRelayMessages()
{
    while (not vRelayExpiration.empty() and vRelayExpiration.front().first < GetTime())
    {
        mapRelay.erase(vRelayExpiration.front().second);
        vRelayExpiration.pop_front();
    }
}

void Network::SaveSerializedMessage(const CInv inv, const CDataStream& ss)
{
    mapRelay.insert(std::make_pair(inv, ss));
    vRelayExpiration.push_back(std::make_pair(GetTime() + 15 * 60, inv));
}

void Network::RelayMessage(const CDataStream& ss, int type)
{
    uint256 hash = Hash(ss.begin(), ss.end());
    CInv inv(type, hash);
    {
        LOCK(cs_mapRelay);
        ExpireOldRelayMessages();
        SaveSerializedMessage(inv, ss);
    }
    LOCK(cs_vNodes);
    for (auto pnode : vNodes)
    {
        if (pnode == vNodes[0] and vNodes.size() > 1)
        {
            continue;
        }
        pnode->PushInventory(inv);
    }
}

void Network::RelayMessage(const CDataStream& ss)
{
    RelayMessage(ss, MSG_GENERAL);
}
