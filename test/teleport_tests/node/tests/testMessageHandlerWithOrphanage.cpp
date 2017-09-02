#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include <src/crypto/uint256.h>
#include <src/crypto/hash.h>
#include <src/net/net_cnode.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/MessageHandlerWithOrphanage.h"
#include "test/teleport_tests/node/TestPeer.h"

using namespace ::testing;
using namespace std;

static uint64_t number_of_test_messages{0};

class TestMessageWithDependencies
{
public:
    uint64_t test_message_number;
    vector<uint160> dependencies;
    vector<uint160> Dependencies() { return dependencies; }

    static string Type() { return "test"; }

    TestMessageWithDependencies() { test_message_number = (number_of_test_messages++); }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(test_message_number);
        READWRITE(dependencies);
    )

    uint160 GetHash160()
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }
};

class AMessageHandlerWithOrphanage : public Test
{
public:
    MemoryDataStore msgdata;
    MessageHandlerWithOrphanage *message_handler;
    TestMessageWithDependencies message1, message2;

    virtual void SetUp()
    {
        message_handler = new MessageHandlerWithOrphanage(msgdata);
        uint160 message1_hash = message1.GetHash160();
        message2.dependencies.push_back(message1_hash);
    }

    virtual void TearDown()
    {
        delete message_handler;
    }
};

TEST_F(AMessageHandlerWithOrphanage, DetectsWhenDependenciesWereNotReceived)
{
    bool received_dependencies = message_handler->CheckDependenciesWereReceived(message2);
    ASSERT_THAT(received_dependencies, Eq(false));
}

TEST_F(AMessageHandlerWithOrphanage, DetectsWhenDependenciesWereReceived)
{
    msgdata[message1.GetHash160()]["received"] = true;
    bool received_dependencies = message_handler->CheckDependenciesWereReceived(message2);
    ASSERT_THAT(received_dependencies, Eq(true));
}

TEST_F(AMessageHandlerWithOrphanage, RegistersTheOrphansOfAGivenMessageHash)
{
    message_handler->RegisterAsOrphanIfAppropriate(message2);
    std::set<uint160> orphans_of_message1 = message_handler->GetOrphans(message1.GetHash160());
    ASSERT_TRUE(orphans_of_message1.count(message2.GetHash160()));
}

TEST_F(AMessageHandlerWithOrphanage, RegistersIncomingMessages)
{
    TestPeer peer;
    message_handler->RegisterIncomingMessage(message1, &peer);
    bool message_received = msgdata[message1.GetHash160()]["received"];
    ASSERT_THAT(message_received, Eq(true));
}

TEST_F(AMessageHandlerWithOrphanage, RecordsTheTypeOfIncomingMessages)
{
    TestPeer peer;
    message_handler->RegisterIncomingMessage(message1, &peer);
    string message_type = msgdata[message1.GetHash160()]["type"];
    ASSERT_THAT(message_type, Eq("test"));
}

TEST_F(AMessageHandlerWithOrphanage, RegistersIncomingMessagesAsOrphansIfTheyAreOrphans)
{
    TestPeer peer;
    message_handler->RegisterIncomingMessage(message2, &peer);
    bool is_orphan = message_handler->IsOrphan(message2.GetHash160());
    ASSERT_THAT(is_orphan, Eq(true));
}

TEST_F(AMessageHandlerWithOrphanage, KnowsAMessageIsNotAnOrphanAfterItsDependenciesHaveBeenReceived)
{
    TestPeer peer;
    message_handler->RegisterIncomingMessage(message2, &peer);
    message_handler->RegisterIncomingMessage(message1, &peer);
    bool is_orphan = message_handler->IsOrphan(message2.GetHash160());
    ASSERT_THAT(is_orphan, Eq(false));
}

TEST_F(AMessageHandlerWithOrphanage, RemembersThePeerWhoSentAMessage)
{
    TestPeer peer;
    peer.id = 2;
    message_handler->RegisterIncomingMessage(message1, &peer);
    NodeId peer_id = msgdata[message1.GetHash160()]["peer"];
    ASSERT_THAT(peer_id, Eq(peer.id));
}

TEST_F(AMessageHandlerWithOrphanage, HandlesMessages)
{
    TestPeer peer;
    message_handler->RegisterIncomingMessage(message1, &peer);
    message_handler->HandleMessage(message1.GetHash160());
    bool message_handled = msgdata[message1.GetHash160()]["handled"];
    ASSERT_THAT(message_handled, Eq(true));
}


class TestMessageHandler : public MessageHandlerWithOrphanage
{
public:
    TestMessageHandler(MemoryDataStore &msgdata) :
            MessageHandlerWithOrphanage(msgdata) { }

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(TestMessageWithDependencies);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(TestMessageWithDependencies);
    }

    HANDLECLASS(TestMessageWithDependencies);
    void HandleTestMessageWithDependencies(TestMessageWithDependencies message)
    {
        msgdata[message.GetHash160()]["handled_by_test_message_handler"] = true;
    }
};


class ATestMessageHandler : public Test
{
public:
    MemoryDataStore msgdata;
    TestMessageHandler *message_handler;
    TestMessageWithDependencies message1, message2;
    uint160 message1_hash, message2_hash;

    TestPeer *peer;

    virtual void SetUp()
    {
        message_handler = new TestMessageHandler(msgdata);
        peer = new TestPeer();
        message1_hash = message1.GetHash160();
        message2.dependencies.push_back(message1_hash);
        message2_hash = message2.GetHash160();
    }

    virtual void TearDown()
    {
        delete message_handler;
        delete peer;
    }
};

TEST_F(ATestMessageHandler, HandlesAnIncomingTestMessage)
{
    message_handler->RegisterIncomingMessage(message1, NULL);
    message_handler->HandleMessage(message1_hash);
    bool handled = msgdata[message1_hash]["handled_by_test_message_handler"];
    ASSERT_THAT(handled, Eq(true));
}

template <typename T>
CDataStream TestDataStream(T message)
{
    CDataStream stream(SER_NETWORK, CLIENT_VERSION);
    stream << string("test") << string("test") << message;
    return stream;
}

TEST_F(ATestMessageHandler, HandlesAnIncomingTestMessageDataStream)
{
    message_handler->HandleMessage(TestDataStream(message1), peer);
    bool handled = msgdata[message1_hash]["handled_by_test_message_handler"];
    ASSERT_THAT(handled, Eq(true));
}

TEST_F(ATestMessageHandler, DoesntHandleAnIncomingMessageDataStreamWithMissingDependencies)
{
    message_handler->HandleMessage(TestDataStream(message2), peer);
    bool handled = msgdata[message2_hash]["handled_by_test_message_handler"];
    ASSERT_THAT(handled, Eq(false));
}

TEST_F(ATestMessageHandler, HandlesAnIncomingMessageAfterMissingDependenciesHaveBeenReceived)
{
    message_handler->HandleMessage(TestDataStream(message2), peer);
    bool handled = msgdata[message2_hash]["handled_by_test_message_handler"];
    ASSERT_THAT(handled, Eq(false));

    message_handler->HandleMessage(TestDataStream(message1), peer);
    handled = msgdata[message2_hash]["handled_by_test_message_handler"];
    ASSERT_THAT(handled, Eq(true));
}

TEST_F(ATestMessageHandler, DoesntHandleAnIncomingMessageDataStreamWithADependencyWhichHasBeenRejected)
{
    message_handler->HandleMessage(TestDataStream(message2), peer);
    msgdata[message1_hash]["rejected"] = true;
    message_handler->HandleMessage(TestDataStream(message1), peer);

    bool handled = msgdata[message2_hash]["handled_by_test_message_handler"];
    ASSERT_THAT(handled, Eq(false));
}

TEST_F(ATestMessageHandler, FetchesMissingDependenciesFromThePeer)
{
    message_handler->HandleMessage(TestDataStream(message2), peer);

    ASSERT_THAT(peer->dependencies_requested.size(), Eq(1));
    ASSERT_THAT(peer->dependencies_requested[0], Eq(message1_hash));
}

TEST_F(ATestMessageHandler, RecoversTheCorrectPeerIfItsStillConnected)
{
    message_handler->SetNetwork(peer->dummy_network);
    message_handler->HandleMessage(TestDataStream(message2), peer);

    CNode *recalled_peer = message_handler->GetPeer(message2_hash);
    ASSERT_THAT(recalled_peer, Eq(peer));
}