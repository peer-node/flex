#ifndef FLEX_MESSAGEHANDLERWITHORPHANAGE_H
#define FLEX_MESSAGEHANDLERWITHORPHANAGE_H

#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include <src/crypto/uint256.h>
#include <src/net/net_cnode.h>


class MessageHandlerWithOrphanage
{
public:
    MemoryDataStore& msgdata;

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

        for (auto dependency : message.dependencies)
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
            msgdata[message_hash]["peer"] = peer->id;

        RegisterAsOrphanIfAppropriate(message);
        RemoveFromMissingDependenciesAndRecordNewNonOrphans(message_hash);
    }

    virtual void HandleMessage(CDataStream ss, CNode *peer)
    {
        std::string message_type;
        ss >> message_type;
        ss >> message_type;

        // populate with handle_stream_
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

    virtual void HandleMessage(uint160 message_hash)
    {
        // populate with handle_hash_

        msgdata[message_hash]["handled"] = true;
    }

    void HandleOrphans(uint160 message_hash)
    {
        std::set<uint160> new_non_orphans = msgdata[message_hash]["new_non_orphans"];
        for (auto non_orphan : new_non_orphans)
        {
            HandleMessage(non_orphan);
            new_non_orphans.erase(non_orphan);
        }
        msgdata[message_hash]["new_non_orphans"] = new_non_orphans;
    }

    std::set<uint160> GetOrphans(uint160 message_hash);

    bool IsOrphan(uint160 message_hash);
};


#define handle_stream_(ClassName, Datastream, Peer)                                 \
        if (message_type == ClassName::Type())                                      \
        {                                                                           \
            Handle ## ClassName ## Stream(Datastream, Peer);                        \
        }

#define handle_hash_(ClassName, Uint160)                                            \
        if (message_type == ClassName::Type())                                      \
            Handle ## ClassName ## Hash(Uint160);

#define HandleClass_(ClassName)                                                     \
    void Handle ## ClassName ## Hash(uint160 message_hash)                          \
    {                                                                               \
        ClassName object;                                                           \
        object = msgdata[message_hash][object.Type()];                              \
        CNode *peer = NULL;                                                         \
        Handle(object, peer);                                                       \
    }                                                                               \
    void Handle ## ClassName ## Stream(CDataStream ss, CNode *peer)                 \
    {                                                                               \
        ClassName object = Get ## ClassName ## FromStream(ss);                      \
        RegisterIncomingMessage(object, peer);                                      \
        if (not IsOrphan(object.GetHash160()))                                      \
            Handle(object, peer);                                                   \
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
        if (not msgdata[instance.GetHash160()]["rejected"])                         \
            HandleOrphans(message_hash);                                            \
    }


#endif //FLEX_MESSAGEHANDLERWITHORPHANAGE_H
