#ifndef FLEX_MESSAGEHANDLERWITHORPHANAGE_H
#define FLEX_MESSAGEHANDLERWITHORPHANAGE_H

#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include <src/crypto/uint256.h>
#include <src/net/net_cnode.h>

#include "log.h"
#define LOG_CATEGORY "MessageHandlerWithOrphanage.h"

class MessageHandlerWithOrphanage
{
public:
    MemoryDataStore& msgdata;
    Network *network{NULL};
    std::string channel{""};

    void SetNetwork(Network &network_)
    {
        network = &network_;
    }

    template <typename T>
    void Broadcast(T message)
    {
        if (network != NULL)
            network->Broadcast(channel, message.Type(), message);
        else
            log_ << "not broadcasting: no network set\n";
    }

    MessageHandlerWithOrphanage(MemoryDataStore &msgdata);

    template<typename T>
    bool CheckDependenciesWereReceived(T message)
    {
        return GetMissingDependencies(message).size() == 0;
    }

    template<typename T>
    std::set<uint160> GetMissingDependencies(T message)
    {
        std::set<uint160> missing_dependencies;

        for (auto dependency : message.Dependencies())
        {
            if (dependency != 0 and not msgdata[dependency]["received"])
                missing_dependencies.insert(dependency);
        }

        return missing_dependencies;
    }

    void AddToOrphans(uint160 orphan, uint160 dependency)
    {
        std::set<uint160> orphans = msgdata[dependency]["orphans"];
        orphans.insert(orphan);
        msgdata[dependency]["orphans"] = orphans;
    }

    template<typename T>
    void RegisterAsOrphanIfAppropriate(T message)
    {
        std::set<uint160> missing_dependencies = GetMissingDependencies(message);
        uint160 message_hash = message.GetHash160();
        msgdata[message_hash]["missing_dependencies"] = missing_dependencies;
        for (auto dependency : missing_dependencies)
            AddToOrphans(message_hash, dependency);
    }

    template<typename T>
    void RegisterIncomingMessage(T message, CNode *peer)
    {
        uint160 message_hash = message.GetHash160();

        msgdata[message_hash]["received"] = true;
        msgdata[message_hash]["type"] = message.Type();
        msgdata[message_hash][message.Type()] = message;
        msgdata[message_hash].Location("received_at") = GetTimeMicros();
        if (peer != NULL)
        {
            msgdata[message_hash]["peer"] = peer->id;
            if (network == NULL)
                network = &peer->network;
        }

        RegisterAsOrphanIfAppropriate(message);
        RemoveFromMissingDependenciesAndRecordNewNonOrphans(message_hash);
    }

    void RemoveFromMissingDependenciesAndRecordNewNonOrphans(uint160 message_hash)
    {
        std::set<uint160> new_non_orphans;
        std::set<uint160> orphans = msgdata[message_hash]["orphans"];
        for (auto orphan : orphans)
        {
            std::set<uint160> missing_dependencies = msgdata[orphan]["missing_dependencies"];
            missing_dependencies.erase(message_hash);
            msgdata[orphan]["missing_dependencies"] = missing_dependencies;
            if (missing_dependencies.size() == 0)
                new_non_orphans.insert(orphan);
        }
        msgdata[message_hash]["new_non_orphans"] = new_non_orphans;
    }

    void HandleOrphans(uint160 message_hash)
    {
        std::set<uint160> new_non_orphans = msgdata[message_hash]["new_non_orphans"];
        for (auto non_orphan : new_non_orphans)
        {
            HandleMessage(non_orphan);
        }
        for (auto non_orphan : new_non_orphans)
        {
            new_non_orphans.erase(non_orphan);
        }
        msgdata[message_hash]["new_non_orphans"] = new_non_orphans;
    }

    std::set<uint160> GetOrphans(uint160 message_hash);

    bool IsOrphan(uint160 message_hash);

    std::string GetMessageTypeFromDataStream(CDataStream ss)
    {
        std::string type;
        ss >> type;
        ss >> type;
        return type;
    }

    bool RejectMessage(uint160 msg_hash)
    {
        msgdata[msg_hash]["rejected"] = true;
        CNode *peer = GetPeer(msg_hash);
        if (peer != NULL)
            peer->Misbehaving(30);
        return false;
    }

    virtual void HandleMessage(uint160 message_hash)
    {
        // populate with HANDLEHASH

        msgdata[message_hash]["handled"] = true;
    }

    virtual void HandleMessage(CDataStream ss, CNode *peer)
    {
        // populate with HANDLESTREAM
    }


    // put one HANDLECLASS here for each message type
    // and implement void HandleMyMessage(MyMessage message)

    CNode *GetPeer(uint160 message_hash)
    {
        NodeId node_id = msgdata[message_hash]["peer"];
        if (network != NULL)
            for (auto peer : network->vNodes)
            {
                if(peer->id == node_id)
                    return peer;
            }
        return NULL;
    }

    template<typename T>
    void SendMessageToPeer(T message, CNode* peer)
    {
        msgdata[message.GetHash160()][message.Type()] = message;
        peer->PushMessage(channel.c_str(), message.Type(), message);
    }
};


#define HANDLESTREAM(ClassName)                                                     \
    {                                                                               \
        if (GetMessageTypeFromDataStream(ss) == ClassName::Type())                  \
        {                                                                           \
            std::string message_type;                                               \
            ss >> message_type;                                                     \
            ss >> message_type;                                                     \
            Handle ## ClassName ## Stream(ss, peer);                                \
            return;                                                                 \
        }                                                                           \
    }

#define HANDLEHASH(ClassName)                                                       \
    {                                                                               \
        std::string type = msgdata[message_hash]["type"];                           \
        if (type == ClassName::Type())                                              \
            Handle ## ClassName ## Hash(message_hash);                              \
    }

#define HANDLECLASS(ClassName)                                                      \
    void Handle ## ClassName ## Hash(uint160 message_hash)                          \
    {                                                                               \
        ClassName object;                                                           \
        object = msgdata[message_hash][object.Type()];                              \
        CNode *peer = GetPeer(message_hash);                                        \
        Handle(object, peer);                                                       \
    }                                                                               \
    void Handle ## ClassName ## Stream(CDataStream ss, CNode *peer)                 \
    {                                                                               \
        ClassName object = Get ## ClassName ## FromStream(ss);                      \
        RegisterIncomingMessage(object, peer);                                      \
        uint160 hash = object.GetHash160();                                         \
        if (not IsOrphan(hash))                                                     \
            Handle(object, peer);                                                   \
        else                                                                        \
            peer->FetchDependencies(msgdata[hash]["missing_dependencies"]);         \
    }                                                                               \
    ClassName Get ## ClassName ## FromStream(CDataStream ss)                        \
    {                                                                               \
        ClassName object;                                                           \
        ss >> object;                                                               \
        return object;                                                              \
    }                                                                               \
    void Handle(ClassName instance, CNode *peer)                                    \
    {                                                                               \
        Handle ## ClassName(instance);                                              \
        uint160 message_hash = instance.GetHash160();                               \
        if (not msgdata[message_hash]["rejected"])                                  \
            HandleOrphans(message_hash);                                            \
        msgdata[message_hash]["handled"] = true;                                    \
    }



#endif //FLEX_MESSAGEHANDLERWITHORPHANAGE_H
