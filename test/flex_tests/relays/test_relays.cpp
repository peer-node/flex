#include <src/crypto/point.h>
#include "gmock/gmock.h"
#include "../flex_data/TestData.h"
#include "RelayState_.h"
#include "RelayQueue.h"
#include "RelayQueueAdmitter.h"
#include "RelayJoinMessage.h"
#include "RelayKeyPublication.h"
#include "RelayKeyShareMessage.h"
#include "RelayKeyShare.h"
#include "RelayKeyDisclosureMessage.h"
#include "RelayKeyDisclosure.h"
#include "RelayMessageHandler.h"
 
using namespace ::testing;

class ARelay : public TestWithGlobalDatabases
{
public:
    Relay relay;
};

TEST_F(ARelay, HasZeroMinedCreditHashByDefault)
{
    ASSERT_THAT(relay.mined_credit_hash, Eq(0));
}

TEST_F(ARelay, HasNumberZeroByDefault)
{
    ASSERT_THAT(relay.number, Eq(0));
}

TEST_F(ARelay, CanBeStoredInADatabase)
{
    relaydata["example"]["relay"] = relay;
}

TEST_F(ARelay, RetainsDataWhenRetrievedFromDatabase)
{
    relay.mined_credit_hash = 1;
    relaydata["example"]["relay"] = relay;
    Relay relay2;
    relay2 = relaydata["example"]["relay"];
    ASSERT_THAT(relay2.mined_credit_hash, Eq(1));
}


class ARelayState_ : public TestWithGlobalDatabases
{
public:
    RelayState_ state;
};

TEST_F(ARelayState_, HasNoRelaysByDefault)
{
    ASSERT_THAT(state.relays.size(), Eq(0));
}

TEST_F(ARelayState_, CanAddRelays)
{
    Relay relay;
    state.AddRelay(relay);
    ASSERT_THAT(state.relays.size(), Eq(1));
}

class ARelayQueue : public TestWithGlobalDatabases
{
public:
    RelayQueue queue;
};

TEST_F(ARelayQueue, HasNoRelaysByDefault)
{
    ASSERT_THAT(queue.relays.size(), Eq(0));
}


class ARelayQueueAdmitter : public TestWithGlobalDatabases
{
public:
    RelayQueueAdmitter queue_admitter;
};

TEST_F(ARelayQueueAdmitter, HasARelayQueue)
{
    RelayQueue queue = queue_admitter.queue;
}

TEST_F(ARelayQueueAdmitter, HasARelayState)
{
    RelayState_ state = queue_admitter.state;
}


class ARelayJoinMessage : public TestWithGlobalDatabases
{
public:
    RelayJoinMessage join_message;
};

TEST_F(ARelayJoinMessage, SpecifiesAMinedCreditHash)
{
    uint160 mined_credit_hash = join_message.mined_credit_hash;
}

TEST_F(ARelayJoinMessage, SpecifiesAPrecedingJoinMessageHash)
{
    uint160 preceding_join_message_hash = join_message.preceding_join_message_hash;
}

TEST_F(ARelayJoinMessage, SpecifiesARelayNumber)
{
    uint32_t relay_number = join_message.relay_number;
}

TEST_F(ARelayJoinMessage, ContainsARelayKeyPublication)
{
    RelayKeyPublication key_publication = join_message.key_publication;
}


TEST(ARelayKeyPublication, SpecifiesFourExecutorKeys)
{
    RelayKeyPublication key_publication;
    Point first_executor_key = key_publication.ExecutorKey(1);
}


class ARelayKeyShareMessage : public Test
{
public:
    RelayKeyShareMessage key_share_message;
};

TEST_F(ARelayKeyShareMessage, SpecifiesAJoinMessageHash)
{
    uint160 join_message_hash = key_share_message.join_message_hash;
}

TEST_F(ARelayKeyShareMessage, ContainsARelayKeyShare)
{
    RelayKeyShare key_share = key_share_message.key_share;
}


class ARelayKeyDisclosureMessage : public Test
{
public:
    RelayKeyDisclosureMessage key_disclosure_message;
};

TEST_F(ARelayKeyDisclosureMessage, SpecifiesAJoinMessageHash)
{
    uint160 join_message_hash = key_disclosure_message.join_message_hash;
}

TEST_F(ARelayKeyDisclosureMessage, ContainsARelayKeyDisclosure)
{
    RelayKeyDisclosure key_disclosure = key_disclosure_message.key_disclosure;
}


TEST(ARelayMessageHandler, HandlesARelayJoinMessage)
{
    RelayMessageHandler relay_message_handler;
    RelayJoinMessage join_message;
    relay_message_handler.HandleRelayJoinMessage(join_message);
}