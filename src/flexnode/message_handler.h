#ifndef FLEX_MESSAGE_HANDLER
#define FLEX_MESSAGE_HANDLER

#include "credits/credits.h"
#include "database/memdb.h"
#include "database/data.h"
#include "net/net.h"

#include "log.h"
#define LOG_CATEGORY "message_handler.h"


class MessageHandlerWithOrphanage
{
public:
    bool should_forward;

    // put one HandleClass_ here for each message type

    virtual void HandleMessage(CDataStream ss, CNode *peer)
    {
        string message_type;
        ss >> message_type;
        ss >> message_type;

        // populate with handle_stream_
    }
    
    virtual void HandleMessage(uint160 message_hash)
    {
        string_t message_type = msgdata[message_hash]["type"];

        // populate with handle_hash_
    }

    void HandleOrphans(uint160 message_hash)
    {
        log_ << "Handling orphans\n";
        
        std::vector<uint160> orphans = msgdata[message_hash]["orphans"];
        std::vector<uint160> new_non_orphans;

        foreach_(const uint160 orphan, orphans)
        {
            log_ << orphan << " is an orphan of " << message_hash << "\n";
                
            set<uint160> parents = msgdata[orphan]["missing_parents"];
            parents.erase(message_hash);
            msgdata[orphan]["missing_parents"] = parents;
            if (parents.size() == 0)
            {
                log_ << orphan << " is not an orphan any more\n";
                new_non_orphans.push_back(orphan);
            }
        }

        foreach_(const uint160 non_orphan, new_non_orphans)
        {
            log_ << "handling non-orphan " << non_orphan << "\n";
            HandleMessage(non_orphan);
        }

        msgdata[message_hash]["orphans"] = std::vector<uint160>();
    }

    CNode *GetPeer(uint160 message_hash)
    {
        int32_t node_id = msgdata[message_hash]["peer"];
        CNode *peer = NULL;
        foreach_(CNode* pnode, vNodes)
        {
            if(pnode->id == node_id)
                peer = pnode;
        }
        return peer;
    }

    void AddToOrphans(uint160 orphan, uint160 missing_parent)
    {
        log_ << "adding " << orphan << " to orphans of " 
             << missing_parent << "\n";
        std::vector<uint160> orphans = msgdata[missing_parent]["orphans"];
        if (!VectorContainsEntry(orphans, orphan))
            orphans.push_back(orphan);
        msgdata[missing_parent]["orphans"] = orphans;

        std::vector<uint160> missing_parents
            = msgdata[orphan]["missing_parents"];
        if (!VectorContainsEntry(missing_parents, missing_parent))
            missing_parents.push_back(missing_parent);

        msgdata[orphan]["missing_parents"] = missing_parents;
    }

    template<typename T>
    bool CheckDependenciesWereReceived(T message)
    {
        uint160 message_hash = message.GetHash160();
        std::vector<uint160> dependencies = message.Dependencies();
        bool is_orphan = false;

        foreach_(const uint160& dependency, dependencies)
        {
            if (dependency != 0 && !msgdata[dependency]["received"])
            {
                log_ << "Dependency: " << dependency << " not received\n";
                AddToOrphans(message_hash, dependency);
                is_orphan = true;
            }
        }
        return !is_orphan;
    }
};


#define handle_stream_(ClassName, Datastream, Peer)                     \
        if (message_type == ClassName::Type())                          \
        {                                                               \
            Handle ## ClassName ## Stream(Datastream, Peer);            \
            ClassName message                                           \
                 = Get ## ClassName ## FromStream(Datastream, Peer);    \
            uint160 hash = message.GetHash160();                        \
            if (should_forward)                                         \
                outgoing_resource_limiter.ScheduleForwarding(hash, 0);  \
        }

#define handle_hash_(ClassName, Uint160)                                \
        if (message_type == ClassName::Type())                          \
            Handle ## ClassName ## Hash(Uint160);

#define HandleClass_(ClassName)                                         \
    void Handle ## ClassName ## Hash(uint160 message_hash)              \
    {                                                                   \
        ClassName object;                                               \
        string_t type = msgdata[message_hash]["type"];                  \
        object = msgdata[message_hash][type];                           \
        StoreHash(message_hash);                                        \
        CNode *peer = GetPeer(object.GetHash160());                     \
        Handle(object, peer);                                           \
    }                                                                   \
    void Handle ## ClassName ## Stream(CDataStream ss, CNode *peer)     \
    {                                                                   \
        ClassName object;                                               \
        ss >> object;                                                   \
        Handle(object, peer);                                           \
    }                                                                   \
    ClassName Get ## ClassName ## FromStream(CDataStream ss,            \
                                             CNode *peer)               \
    {                                                                   \
        ClassName object;                                               \
        ss >> object;                                                   \
        return object;                                                  \
    }                                                                   \
    void Handle(ClassName instance, CNode *peer)                        \
    {                                                                   \
        should_forward = true;                                          \
        uint160 message_hash = instance.GetHash160();                   \
        StoreHash(message_hash);                                        \
        uint64_t now = GetTimeMicros();                                 \
        string_t type = instance.Type();                                \
        log_ << "HANDLE: " << message_hash                              \
             << " has type " << type << "\n";                           \
        msgdata[message_hash]["type"] = type;                           \
        msgdata[message_hash][type] = instance;                         \
        msgdata[message_hash]["received"] = true;                       \
        msgdata[message_hash].Location("received_at") = now;            \
        if (peer != NULL)                                               \
            msgdata[message_hash]["peer"] = peer->id;                   \
        if (!CheckDependenciesWereReceived(instance))                   \
        {                                                               \
            log_ << "Some dependencies not received\n";                 \
            FetchDependencies(instance.Dependencies(), peer);           \
            return;                                                     \
        }                                                               \
        Handle ## ClassName(instance);                                  \
        if (should_forward ||                                           \
            tradedata[message_hash].HasProperty("forwarding"))          \
            HandleOrphans(message_hash);                                \
    }

#endif