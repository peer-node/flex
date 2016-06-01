#include "net.h"
#include "net_cnode.h"
#include "net_relay.h"


void Network::RelayTransaction(const SignedTransaction& tx)
{
    RelayTransaction(tx, tx.GetHash());
}

void Network::RelayTransaction(const SignedTransaction& tx, const uint256& hash)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << tx;
    RelayTransaction(tx, hash, ss);
}

void Network::RelayTransaction(const SignedTransaction& tx, const uint256& hash, const CDataStream& ss)
{
    CInv inv(MSG_TX, hash);
    {
        LOCK(cs_mapRelay);
        ExpireOldRelayMessages();
        SaveSerializedMessage(inv, ss);
    }
    LOCK(cs_vNodes);
    for (auto pnode : vNodes)
        pnode->PushInventory(inv);
}

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

void Network::RelayMinedCredit(const MinedCreditMessage& message)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << message;
    uint256 hash = Hash(ss.begin(), ss.end());

    RelayMinedCredit(message, hash, ss);
}

void Network::ExpireOldRelayMessages()
{
    while (!vRelayExpiration.empty() &&
           vRelayExpiration.front().first < GetTime())
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

void Network::RelayMinedCredit(const MinedCreditMessage& message, const uint256& hash, const CDataStream& ss)
{
    CInv inv(MSG_BLOCK, hash);
    {
        LOCK(cs_mapRelay);
        ExpireOldRelayMessages();
        SaveSerializedMessage(inv, ss);
    }
    LOCK(cs_vNodes);
    for(auto pnode : vNodes)
        pnode->PushInventory(inv);
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
        pnode->PushInventory(inv);
}

void Network::RelayMessage(const CDataStream& ss)
{
    RelayMessage(ss, MSG_GENERAL);
}

void Network::RelayTradeMessage(const CDataStream& ss)
{
    RelayMessage(ss, MSG_TRADE);
}

void Network::RelayDepositMessage(const CDataStream& ss)
{
    RelayMessage(ss, MSG_DEPOSIT);
}

void Network::RelayRelayMessage(const CDataStream& ss)
{
    RelayMessage(ss, MSG_RELAY);
}
