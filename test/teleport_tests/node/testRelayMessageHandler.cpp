#include "gmock/gmock.h"
#include "TestPeer.h"
#include "RelayMessageHandler.h"
#include "RelayState.h"
#include "CreditSystem.h"


using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ARelayMessageHandler : public Test
{
public:
    MemoryDataStore msgdata, creditdata, keydata;
    CreditSystem *credit_system;
    RelayMessageHandler *relay_message_handler;
    Calendar calendar;
    TestPeer peer;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        relay_message_handler = new RelayMessageHandler(msgdata, creditdata, keydata);
        relay_message_handler->SetCreditSystem(credit_system);
        relay_message_handler->SetCalendar(&calendar);
    }

    virtual void TearDown()
    {
        delete relay_message_handler;
        delete credit_system;
    }

    RelayJoinMessage ARelayJoinMessageWithAValidSignature();

    MinedCreditMessage AMinedCreditMessageWithNonZeroTotalWork();
};

TEST_F(ARelayMessageHandler, StartsWithNoRelaysInItsState)
{
    RelayState relay_state = relay_message_handler->relay_state;
    ASSERT_THAT(relay_state.relays.size(), Eq(0));
}

MinedCreditMessage ARelayMessageHandler::AMinedCreditMessageWithNonZeroTotalWork()
{
    MinedCreditMessage msg;
    msg.mined_credit.network_state.difficulty = 10;
    msg.mined_credit.keydata = Point(SECP256K1, 5).getvch();
    keydata[Point(SECP256K1, 5)]["privkey"] = CBigNum(5);
    return msg;
}

TEST_F(ARelayMessageHandler, DetectsWhetherTheMinedCreditMessageHashInARelayJoinMessageIsInTheMainChain)
{
    RelayJoinMessage relay_join_message;
    auto msg = AMinedCreditMessageWithNonZeroTotalWork();

    relay_join_message.mined_credit_message_hash = msg.GetHash160();
    bool in_main_chain = relay_message_handler->MinedCreditMessageHashIsInMainChain(relay_join_message);
    ASSERT_THAT(in_main_chain, Eq(false));

    credit_system->AddToMainChain(msg);
    in_main_chain = relay_message_handler->MinedCreditMessageHashIsInMainChain(relay_join_message);
    ASSERT_THAT(in_main_chain, Eq(true));
}


RelayJoinMessage ARelayMessageHandler::ARelayJoinMessageWithAValidSignature()
{
    auto msg = AMinedCreditMessageWithNonZeroTotalWork();
    credit_system->StoreMinedCreditMessage(msg);
    RelayJoinMessage relay_join_message;
    relay_join_message.mined_credit_message_hash = msg.GetHash160();
    relay_join_message.Sign(Databases(msgdata, creditdata, keydata));
    return relay_join_message;
}

TEST_F(ARelayMessageHandler, UsesTheMinedCreditMessagePublicKeyToCheckTheSignatureInARelayJoinMessage)
{
    RelayJoinMessage signed_join_message = ARelayJoinMessageWithAValidSignature();
    bool ok = signed_join_message.VerifySignature(Databases(msgdata, creditdata, keydata));
    ASSERT_THAT(ok, Eq(true));
}