#include <src/base/util_hex.h>
#include <test/teleport_tests/node/credit_handler/MinedCreditMessageBuilder.h>
#include "gmock/gmock.h"
#include "TestPeer.h"
#include "test/teleport_tests/node/relays/RelayMessageHandler.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ARelayTipHandler : public Test
{
public:
    MemoryDataStore msgdata, creditdata, keydata;
    CreditSystem *credit_system;
    RelayMessageHandler *relay_message_handler;
    RelayTipHandler *relay_tip_handler;
    RelayState *relay_state;
    Calendar calendar;
    Data *data;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        relay_message_handler = new RelayMessageHandler(Data(msgdata, creditdata, keydata));
        relay_message_handler->SetCreditSystem(credit_system);
        relay_message_handler->SetCalendar(&calendar);
        relay_tip_handler = &relay_message_handler->tip_handler;

        data = &relay_message_handler->data;

        relay_message_handler->admission_handler.scheduler.running = false;
        relay_message_handler->succession_handler.scheduler.running = false;

        relay_state = &relay_message_handler->relay_state;
    }

    virtual void TearDown()
    {
        delete relay_message_handler;
        delete credit_system;
    }

    template <typename T>
    CDataStream GetDataStream(T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << std::string("relay") << message.Type() << message;
        return ss;
    }
};

class ARelayTipHandlerWithAJoinMessageInTheObservedHistory : public ARelayTipHandler
{
public:
    RelayJoinMessage join_message;
    uint160 join_message_hash;

    virtual void SetUp()
    {
        ARelayTipHandler::SetUp();
        GenerateJoinMessage();
        relay_tip_handler->unencoded_observed_history.push_back(join_message_hash);
    }

    virtual void TearDown()
    {
        ARelayTipHandler::TearDown();
    }

    MinedCreditMessage AMinedCreditMessageContainingAJoinMessage()
    {
        MinedCreditMessage msg;
        msg.hash_list.full_hashes.push_back(join_message_hash);
        msg.hash_list.GenerateShortHashes();
        data->StoreMessage(msg);
        return msg;
    }

    void GenerateJoinMessage()
    {
        join_message = Relay().GenerateJoinMessage(keydata, 1);
        join_message_hash = join_message.GetHash160();
        data->StoreMessage(join_message);
    }
};


TEST_F(ARelayTipHandlerWithAJoinMessageInTheObservedHistory,
       RemovesAJoinMessageFromTheUnencodedObservedHistoryWhenAddingAMinedCreditMessageWhichContainsTheJoinMessage)
{
    ASSERT_TRUE(VectorContainsEntry(relay_tip_handler->unencoded_observed_history, join_message_hash));
    MinedCreditMessage msg = AMinedCreditMessageContainingAJoinMessage();
    msg.hash_list.RecoverFullHashes(msgdata);
    relay_tip_handler->RemoveEncodedEventsFromUnencodedObservedHistory(msg.hash_list.full_hashes);
    ASSERT_FALSE(VectorContainsEntry(relay_tip_handler->unencoded_observed_history, join_message_hash));
}
