#include <src/credits/creditsign.h>
#include "gmock/gmock.h"
#include "MessageHandlerWithOrphanage.h"
#include "CreditMessageHandler.h"
#include "TestPeer.h"
#include "Calendar.h"

using namespace ::testing;
using namespace std;

class ACreditMessageHandler : public Test
{
public:
    MemoryDataStore msgdata, creditdata, keydata;
    CreditSystem *credit_system;
    CreditMessageHandler *credit_message_handler;
    Calendar calendar;
    BitChain spent_chain;
    TestPeer peer;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        credit_message_handler = new CreditMessageHandler(msgdata, creditdata, keydata);
        credit_message_handler->SetCreditSystem(credit_system);
        credit_message_handler->SetCalendar(calendar);
        credit_message_handler->SetSpentChain(spent_chain);
    }

    virtual void TearDown()
    {
        delete credit_message_handler;
        delete credit_system;
    }
};

TEST_F(ACreditMessageHandler, StartsWithNoAcceptedMessages)
{
    ASSERT_THAT(credit_message_handler->accepted_messages.size(), Eq(0));
}

template <typename T>
CDataStream GetDataStream(T message)
{
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << std::string("credit") << message.Type() << message;
    return ss;
}

EncodedNetworkState ValidFirstNetworkState()
{
    EncodedNetworkState network_state;

    network_state.network_id = 1;
    network_state.difficulty = INITIAL_DIFFICULTY;
    network_state.diurnal_difficulty = INITIAL_DIURNAL_DIFFICULTY;
    network_state.previous_diurn_root = 0;
    network_state.diurnal_block_root = Diurn().BlockRoot();
    network_state.batch_number = 1;
    network_state.batch_offset = 0;
    network_state.batch_size = 0;
    network_state.batch_root = CreditBatch(0, 0).Root();
    network_state.message_list_hash = ShortHashList<uint160>().GetHash160();
    network_state.previous_mined_credit_hash = 0;
    network_state.spent_chain_hash = BitChain().GetHash160();
    network_state.timestamp = GetTimeMicros();

    return network_state;
}

MinedCreditMessage ValidFirstMinedCreditMessage(CreditSystem* credit_system)
{
    MinedCreditMessage msg;
    msg.mined_credit.amount = ONE_CREDIT;
    msg.mined_credit.keydata = Point(SECP256K1, 2).getvch();
    msg.mined_credit.network_state = ValidFirstNetworkState();
    credit_system->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    credit_system->StoreMinedCreditMessage(msg);
    return msg;
}

TEST_F(ACreditMessageHandler, AcceptsAValidFirstMinedCredit)
{
    MinedCreditMessage msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    bool rejected = msgdata[msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(false));
}

TEST_F(ACreditMessageHandler, RequestsDataIfTheListExpansionFailsForAnIncomingMinedCreditMessage)
{
    MinedCreditMessage msg = ValidFirstMinedCreditMessage(credit_system);
    msg.hash_list.full_hashes.push_back(1);
    msg.hash_list.GenerateShortHashes();
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    ASSERT_THAT(peer.list_expansion_failed_requests.size(), Eq(1));
    ASSERT_THAT(peer.list_expansion_failed_requests[0], Eq(msg.GetHash160()));
}

TEST_F(ACreditMessageHandler, RejectsAMinedCreditMessageIfTheAmountIsNotOneCredit)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    msg.mined_credit.amount = ONE_CREDIT + 1;
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    bool rejected = msgdata[msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ACreditMessageHandler, MarksThePeerAsMisbehavingWhenAMinedCreditMessageIsRejected)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    msg.mined_credit.amount = ONE_CREDIT + 1;
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    ASSERT_THAT(peer.recent_misbehavior, Gt(0));
}

TEST_F(ACreditMessageHandler, RejectsAMinedCreditMessageIfTheNetworkIdDoesntMatchTheConfig)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    msg.mined_credit.network_state.network_id = 2;
    ASSERT_THAT(credit_message_handler->config.Uint64("network_id"), Eq(1));
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    bool rejected = msgdata[msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ACreditMessageHandler, RequiresAMinedCreditMessageWithABatchNumberOfZeroToBeTheStartOfAChain)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    msg.mined_credit.network_state.batch_offset = 1;
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    bool rejected = msgdata[msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
    msg.mined_credit.network_state.batch_offset = 0;

    msgdata[msg.GetHash160()]["rejected"] = false;
    msg.mined_credit.network_state.previous_mined_credit_hash = 1;
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    rejected = msgdata[msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
    msg.mined_credit.network_state.previous_mined_credit_hash = 0;

    msgdata[msg.GetHash160()]["rejected"] = false;
    msg.mined_credit.network_state.difficulty += 1;
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    rejected = msgdata[msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
    msg.mined_credit.network_state.difficulty -= 1;

    msgdata[msg.GetHash160()]["rejected"] = false;
    msg.mined_credit.network_state.diurnal_difficulty += 1;
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    rejected = msgdata[msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ACreditMessageHandler, DoesAQuickCheckOfTheProofOfWork)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_system->creditdata[msg.GetHash160()]["quickcheck_ok"] = false;
    credit_system->creditdata[msg.GetHash160()]["quickcheck_bad"] = true;
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    bool rejected = msgdata[msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
}

TEST_F(ACreditMessageHandler, AddsAValidMinedCreditMessageToTheTipOfTheCalendar)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    ASSERT_THAT(credit_message_handler->calendar->current_diurn.Size(), Eq(0));
    credit_message_handler->AddToTip(msg);
    ASSERT_THAT(credit_message_handler->calendar->current_diurn.Size(), Eq(1));
}

MinedCreditMessage ValidSecondMinedCreditMessage(CreditSystem* credit_system, MinedCreditMessage& prev_msg)
{
    MinedCreditMessage msg;

    msg.mined_credit.amount = ONE_CREDIT;
    msg.mined_credit.keydata = Point(SECP256K1, 3).getvch();

    credit_system->StoreMinedCreditMessage(prev_msg);
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(prev_msg.mined_credit);
    msg.hash_list.full_hashes.push_back(prev_msg.mined_credit.GetHash160());
    msg.hash_list.GenerateShortHashes();
    credit_system->SetBatchRootAndSizeAndMessageListHash(msg);
    msg.mined_credit.network_state.timestamp = GetTimeMicros();
    credit_system->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    credit_system->StoreMinedCreditMessage(msg);
    return msg;
}

TEST_F(ACreditMessageHandler, AcceptsASecondValidMinedCreditMessage)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    auto next_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->HandleMessage(GetDataStream(next_msg), &peer);
    bool rejected = msgdata[next_msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(false));
}

TEST_F(ACreditMessageHandler, AddsASecondMinedCreditMessageToTheTipOfTheCalendar)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->AddToTip(msg);
    auto next_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->AddToTip(next_msg);

    ASSERT_THAT(credit_message_handler->calendar->current_diurn.Size(), Eq(2));
}

TEST_F(ACreditMessageHandler, AddsToTheSpentChainWhenAddingAMinedCreditMessage)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->AddToTip(msg);
    auto next_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->AddToTip(next_msg);

    BitChain spent_chain;
    spent_chain.Add();

    ASSERT_THAT(credit_message_handler->spent_chain->GetHash160(), Eq(spent_chain.GetHash160()));
}

SignedTransaction ValidTransaction(CreditSystem *credit_system, MinedCreditMessage& prev_msg)
{
    CreditBatch batch = credit_system->ReconstructBatch(prev_msg);

    Credit credit = batch.GetCredit(0);

    CreditInBatch credit_in_batch = batch.GetCreditInBatch(credit);

    UnsignedTransaction rawtx;
    rawtx.inputs.push_back(credit_in_batch);
    Credit output = Credit(Point(SECP256K1, 5).getvch(), ONE_CREDIT);
    rawtx.outputs.push_back(output);

    MemoryDataStore keydata;
    keydata[Point(SECP256K1, 2)]["privkey"] = CBigNum(2);
    SignedTransaction tx = SignTransaction(rawtx, keydata);

    return tx;
}

TEST_F(ACreditMessageHandler, HandlesAValidTransaction)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->AddToTip(msg);
    auto second_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->AddToTip(second_msg);

    SignedTransaction tx = ValidTransaction(credit_system , second_msg);

    credit_message_handler->HandleMessage(GetDataStream(tx), &peer);

    bool accepted = VectorContainsEntry(credit_message_handler->accepted_messages, tx.GetHash160());
    ASSERT_THAT(accepted, Eq(true));
}

MinedCreditMessage ValidThirdMinedCreditMessageWithATransaction(CreditSystem* credit_system,
                                                                MinedCreditMessage& prev_msg)
{
    MinedCreditMessage msg;

    msg.mined_credit.amount = ONE_CREDIT;
    msg.mined_credit.keydata = Point(SECP256K1, 4).getvch();

    credit_system->StoreMinedCreditMessage(prev_msg);
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(prev_msg.mined_credit);
    auto tx = ValidTransaction(credit_system, prev_msg);
    credit_system->StoreTransaction(tx);
    msg.hash_list.full_hashes.push_back(prev_msg.mined_credit.GetHash160());
    msg.hash_list.full_hashes.push_back(tx.GetHash160());
    msg.hash_list.GenerateShortHashes();
    credit_system->SetBatchRootAndSizeAndMessageListHash(msg);
    msg.mined_credit.network_state.timestamp = GetTimeMicros();
    credit_system->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    credit_system->StoreMinedCreditMessage(msg);
    return msg;
}

TEST_F(ACreditMessageHandler, AddsToTheSpentChainWhenAddingAMinedCreditMessageContainingATransaction)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->AddToTip(msg);
    auto second_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->AddToTip(second_msg);
    credit_message_handler->HandleSignedTransaction(ValidTransaction(credit_system, second_msg));
    auto third_msg = ValidThirdMinedCreditMessageWithATransaction(credit_system, second_msg);
    credit_message_handler->AddToTip(third_msg);

    BitChain spent_chain;
    spent_chain.Add(); spent_chain.Add(); spent_chain.Add();

    spent_chain.Set(0);

    ASSERT_THAT(credit_message_handler->spent_chain->GetHash160(), Eq(spent_chain.GetHash160()));
}


class ACreditMessageHandlerWithTwoPeers : public ACreditMessageHandler
{
public:
    TestPeer other_peer;

    virtual void SetUp()
    {
        ACreditMessageHandler::SetUp();
        peer.dummy_network.vNodes.push_back(&other_peer);
    }

    virtual void TearDown()
    {
        ACreditMessageHandler::TearDown();
    }
};

TEST_F(ACreditMessageHandlerWithTwoPeers, RelaysAValidFirstMinedCredit)
{
    MinedCreditMessage msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    bool received = other_peer.HasBeenInformedAbout("credit", "msg", msg);
    ASSERT_THAT(received, Eq(true));
}


TEST_F(ACreditMessageHandlerWithTwoPeers, RelaysAValidTransaction)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->AddToTip(msg);
    auto second_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->AddToTip(second_msg);

    auto tx = ValidTransaction(credit_system , second_msg);
    credit_message_handler->HandleMessage(GetDataStream(tx), &peer);

    ASSERT_TRUE(other_peer.HasBeenInformedAbout("credit", "tx", tx));
}
