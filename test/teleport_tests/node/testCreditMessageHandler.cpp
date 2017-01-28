#include <src/credits/creditsign.h>
#include <test/teleport_tests/mining/MiningHashTree.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/credit_handler/CreditMessageHandler.h"
#include "TestPeer.h"
#include "test/teleport_tests/node/credit_handler/ListExpansionRequestMessage.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"

class ACreditMessageHandler : public Test
{
public:
    MemoryDataStore msgdata, creditdata, keydata;
    CreditSystem *credit_system;
    CreditMessageHandler *credit_message_handler;
    TipController *tip_controller;
    MinedCreditMessageBuilder *builder;
    Calendar calendar;
    BitChain spent_chain;
    Wallet *wallet;
    TestPeer peer;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        credit_system->initial_difficulty = 10000;
        credit_message_handler = new CreditMessageHandler(msgdata, creditdata, keydata);
        tip_controller = new TipController(msgdata, creditdata, keydata);
        builder = new MinedCreditMessageBuilder(msgdata, creditdata, keydata);
        credit_message_handler->SetTipController(tip_controller);
        credit_message_handler->SetMinedCreditMessageBuilder(builder);
        credit_message_handler->SetCreditSystem(credit_system);
        credit_message_handler->SetCalendar(calendar);
        credit_message_handler->SetSpentChain(spent_chain);
        wallet = new Wallet(keydata);
        credit_message_handler->SetWallet(*wallet);
    }

    virtual void TearDown()
    {
        delete credit_message_handler;
        delete credit_system;
        delete tip_controller;
        delete builder;
        delete wallet;
    }
};

TEST_F(ACreditMessageHandler, StartsWithNoAcceptedMessages)
{
    ASSERT_THAT(credit_message_handler->builder->accepted_messages.size(), Eq(0));
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
    network_state.difficulty = 10000;
    network_state.diurnal_difficulty = INITIAL_DIURNAL_DIFFICULTY;
    network_state.previous_diurn_root = 0;
    network_state.diurnal_block_root = Diurn().BlockRoot();
    network_state.batch_number = 1;
    network_state.batch_offset = 0;
    network_state.batch_size = 0;
    network_state.batch_root = CreditBatch(0, 0).Root();
    network_state.message_list_hash = ShortHashList<uint160>().GetHash160();
    network_state.previous_mined_credit_message_hash = 0;
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

    ASSERT_TRUE(peer.HasReceived("credit", "list_expansion_request", ListExpansionRequestMessage(msg)));
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
    msg.mined_credit.network_state.previous_mined_credit_message_hash = 1;
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    rejected = msgdata[msg.GetHash160()]["rejected"];
    ASSERT_THAT(rejected, Eq(true));
    msg.mined_credit.network_state.previous_mined_credit_message_hash = 0;

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

MinedCreditMessage AddValidProofOfWork(MinedCreditMessage& msg)
{
    NetworkSpecificProofOfWork enclosed_proof;
    enclosed_proof.branch.push_back(msg.mined_credit.GetHash());
    enclosed_proof.branch.push_back(2);
    uint256 memory_seed = MiningHashTree::EvaluateBranch(enclosed_proof.branch);
    uint64_t memory_factor = MemoryFactorFromNumberOfMegabytes(1);
    TwistWorkProof proof(memory_seed, memory_factor, msg.mined_credit.network_state.difficulty);
    uint8_t keep_working = 1;
    proof.DoWork(&keep_working);
    enclosed_proof.proof = proof;
    msg.proof_of_work = enclosed_proof;
    return msg;
}

MinedCreditMessage MessageWithAValidProofOfWork(CreditSystem* credit_system)
{
    MinedCreditMessage msg = ValidFirstMinedCreditMessage(credit_system);
    msg = AddValidProofOfWork(msg);
    return msg;
}

TEST_F(ACreditMessageHandler, DoesASpotCheckOfTheProofOfWork)
{
    auto msg = MessageWithAValidProofOfWork(credit_system);
    bool ok = credit_message_handler->ProofOfWorkPassesSpotCheck(msg);
    ASSERT_THAT(ok, Eq(true));
}

MinedCreditMessage MessageWithAProofOfWorkThatFailsSpotChecks(CreditSystem* credit_system)
{
    auto msg = MessageWithAValidProofOfWork(credit_system);
    for (int i = 0; i < msg.proof_of_work.proof.links.size(); i++)
        msg.proof_of_work.proof.links[i] += 1;
    credit_system->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    return msg;
}

TEST_F(ACreditMessageHandler, FailsASpotCheckOfTheProofOfWorkIfTheProofIsInvalid)
{
    auto msg = MessageWithAProofOfWorkThatFailsSpotChecks(credit_system);
    bool ok{true};
    while (ok)
        ok = credit_message_handler->ProofOfWorkPassesSpotCheck(msg);
    ASSERT_THAT(ok, Eq(false));
}

TEST_F(ACreditMessageHandler, GeneratesABadBatchMessageForAnInvalidProofOfWork)
{
    auto msg = MessageWithAProofOfWorkThatFailsSpotChecks(credit_system);
    bool ok{true};
    while (ok)
        ok = credit_message_handler->ProofOfWorkPassesSpotCheck(msg);
    BadBatchMessage bad_batch_message = credit_message_handler->GetBadBatchMessage(msg.GetHash160());
    ASSERT_THAT(bad_batch_message.mined_credit_message_hash, Eq(msg.GetHash160()));
    ASSERT_THAT(bad_batch_message.check.VerifyInvalid(msg.proof_of_work.proof), Eq(true));
}

TEST_F(ACreditMessageHandler, AddsAValidMinedCreditMessageToTheTipOfTheCalendar)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    ASSERT_THAT(credit_message_handler->calendar->current_diurn.Size(), Eq(0));
    credit_message_handler->tip_controller->AddToTip(msg);
    ASSERT_THAT(credit_message_handler->calendar->current_diurn.Size(), Eq(1));
}

MinedCreditMessage ValidSecondMinedCreditMessage(CreditSystem* credit_system, MinedCreditMessage& prev_msg)
{
    MinedCreditMessage msg;

    msg.mined_credit.amount = ONE_CREDIT;
    msg.mined_credit.keydata = Point(SECP256K1, 3).getvch();

    credit_system->StoreMinedCreditMessage(prev_msg);
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(prev_msg);
    msg.hash_list.full_hashes.push_back(prev_msg.GetHash160());
    msg.hash_list.GenerateShortHashes();
    credit_system->SetBatchRootAndSizeAndMessageListHashAndSpentChainHash(msg);
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
    credit_message_handler->tip_controller->AddToTip(msg);
    auto next_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->tip_controller->AddToTip(next_msg);

    ASSERT_THAT(credit_message_handler->calendar->current_diurn.Size(), Eq(2));
}

TEST_F(ACreditMessageHandler, AddsToTheSpentChainWhenAddingAMinedCreditMessage)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->tip_controller->AddToTip(msg);
    auto next_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->tip_controller->AddToTip(next_msg);

    BitChain spent_chain;
    spent_chain.Add();

    ASSERT_THAT(credit_message_handler->spent_chain->GetHash160(), Eq(spent_chain.GetHash160()));
}

SignedTransaction ValidTransaction(CreditSystem *credit_system, MinedCreditMessage& prev_msg)
{
    CreditBatch batch = credit_system->ReconstructBatch(prev_msg);

    Credit credit = batch.credits[0];

    CreditInBatch credit_in_batch = batch.GetCreditInBatch(credit);

    UnsignedTransaction rawtx;
    rawtx.inputs.push_back(credit_in_batch);
    Credit output = Credit(Point(SECP256K1, 5).getvch(), ONE_CREDIT * 0.5);
    rawtx.outputs.push_back(output);

    MemoryDataStore keydata;
    keydata[Point(SECP256K1, 2)]["privkey"] = CBigNum(2);
    SignedTransaction tx = SignTransaction(rawtx, keydata);

    return tx;
}

TEST_F(ACreditMessageHandler, HandlesAValidTransaction)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->tip_controller->AddToTip(msg);
    auto second_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->tip_controller->AddToTip(second_msg);

    SignedTransaction tx = ValidTransaction(credit_system , second_msg);

    credit_message_handler->HandleMessage(GetDataStream(tx), &peer);

    bool accepted = VectorContainsEntry(credit_message_handler->builder->accepted_messages, tx.GetHash160());
    ASSERT_THAT(accepted, Eq(true));
}

MinedCreditMessage ValidThirdMinedCreditMessageWithATransaction(CreditSystem* credit_system,
                                                                MinedCreditMessage& prev_msg)
{
    MinedCreditMessage msg;

    msg.mined_credit.amount = ONE_CREDIT;
    msg.mined_credit.keydata = Point(SECP256K1, 4).getvch();

    credit_system->StoreMinedCreditMessage(prev_msg);
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(prev_msg);
    auto tx = ValidTransaction(credit_system, prev_msg);
    credit_system->StoreTransaction(tx);
    msg.hash_list.full_hashes.push_back(prev_msg.GetHash160());
    msg.hash_list.full_hashes.push_back(tx.GetHash160());
    msg.hash_list.GenerateShortHashes();
    credit_system->SetBatchRootAndSizeAndMessageListHashAndSpentChainHash(msg);
    msg.mined_credit.network_state.timestamp = GetTimeMicros();
    credit_system->creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    credit_system->StoreMinedCreditMessage(msg);
    return msg;
}

TEST_F(ACreditMessageHandler, AddsToTheSpentChainWhenAddingAMinedCreditMessageContainingATransaction)
{
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_message_handler->tip_controller->AddToTip(msg);
    auto second_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->tip_controller->AddToTip(second_msg);
    credit_message_handler->HandleSignedTransaction(ValidTransaction(credit_system, second_msg));
    auto third_msg = ValidThirdMinedCreditMessageWithATransaction(credit_system, second_msg);
    credit_message_handler->tip_controller->AddToTip(third_msg);

    BitChain spent_chain;
    spent_chain.Add(); spent_chain.Add(); spent_chain.Add();

    spent_chain.Set(0);

    ASSERT_THAT(credit_message_handler->spent_chain->GetHash160(), Eq(spent_chain.GetHash160()));
}


MinedCreditMessage ValidThirdMinedCreditMessageWithATransactionAndProof(CreditSystem* credit_system,
                                                                        MinedCreditMessage& prev_msg)
{
    auto msg = ValidThirdMinedCreditMessageWithATransaction(credit_system, prev_msg);
    msg = AddValidProofOfWork(msg);
    return msg;
}

void MakeProofOfWorkInvalid(TwistWorkProof& proof)
{
    for (int i = 0; i < proof.links.size(); i++)
        proof.links[i] += 1;
}

void AddThreeMinedCreditMessagesToTheTip(CreditMessageHandler* credit_message_handler)
{
    CreditSystem* credit_system = credit_message_handler->credit_system;
    auto msg = ValidFirstMinedCreditMessage(credit_system);
    credit_system->AcceptMinedCreditMessageAsValidByRecordingTotalWorkAndParent(msg);
    credit_message_handler->tip_controller->AddToTip(msg);

    auto second_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->tip_controller->AddToTip(second_msg);
    credit_system->AcceptMinedCreditMessageAsValidByRecordingTotalWorkAndParent(second_msg);
    credit_message_handler->HandleSignedTransaction(ValidTransaction(credit_system, second_msg));

    auto third_msg = ValidThirdMinedCreditMessageWithATransactionAndProof(credit_system, second_msg);
    MakeProofOfWorkInvalid(third_msg.proof_of_work.proof);
    credit_system->AcceptMinedCreditMessageAsValidByRecordingTotalWorkAndParent(third_msg);
    credit_message_handler->tip_controller->AddToTip(third_msg);
}

TEST_F(ACreditMessageHandler, RemovesBatchesFromMainChainAndSwitchesToANewTipInResponseToAValidBadBatchMessage)
{
    AddThreeMinedCreditMessagesToTheTip(credit_message_handler);
    auto third_msg = credit_message_handler->calendar->current_diurn.credits_in_diurn.back();
    uint160 second_msg_hash = third_msg.mined_credit.network_state.previous_mined_credit_message_hash;

    while (credit_message_handler->ProofOfWorkPassesSpotCheck(third_msg)) { }

    auto bad_batch_message = credit_message_handler->GetBadBatchMessage(third_msg.GetHash160());

    credit_message_handler->HandleBadBatchMessage(bad_batch_message);
    ASSERT_THAT(credit_message_handler->calendar->LastMinedCreditMessageHash(), Eq(second_msg_hash));
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
    credit_message_handler->tip_controller->AddToTip(msg);
    auto second_msg = ValidSecondMinedCreditMessage(credit_system, msg);
    credit_message_handler->tip_controller->AddToTip(second_msg);

    auto tx = ValidTransaction(credit_system , second_msg);
    credit_message_handler->HandleMessage(GetDataStream(tx), &peer);

    ASSERT_TRUE(other_peer.HasBeenInformedAbout("credit", "tx", tx));
}

TEST_F(ACreditMessageHandlerWithTwoPeers, RelaysABadBatchMessage)
{
    auto msg = MessageWithAProofOfWorkThatFailsSpotChecks(credit_system);
    bool ok{true};
    while (ok)
        ok = credit_message_handler->ProofOfWorkPassesSpotCheck(msg);
    BadBatchMessage bad_batch_message = credit_message_handler->GetBadBatchMessage(msg.GetHash160());
    credit_message_handler->HandleMessage(GetDataStream(msg), &peer);
    ASSERT_TRUE(other_peer.HasBeenInformedAbout("credit", "bad_batch", bad_batch_message));
}


class ACreditMessageHandlerWithAcceptedTransactions : public ACreditMessageHandlerWithTwoPeers
{
public:
    vector<MinedCreditMessage> msgs;
    uint64_t n{2};
    MinedCreditMessage message_containing_one_transaction, message_containing_double_spend;
    SignedTransaction first_transaction, second_transaction, double_spend_of_first_transaction;

    Point GetNextPubKey()
    {
        return wallet->GetNewPublicKey();
    }

    MinedCreditMessage NextMinedCreditMessage(MinedCreditMessage& prev_msg)
    {
        MinedCreditMessage msg;
        if (prev_msg.mined_credit.network_state.batch_number != 0)
            msg.hash_list.full_hashes.push_back(prev_msg.GetHash160());
        msg.hash_list.GenerateShortHashes();
        msg.mined_credit.network_state = credit_system->SucceedingNetworkState(prev_msg);

        msg.mined_credit.keydata = GetNextPubKey().getvch();

        credit_system->SetBatchRootAndSizeAndMessageListHashAndSpentChainHash(msg);
        credit_system->StoreMinedCreditMessage(msg);
        return msg;
    }

    SignedTransaction GetTransactionUsingMinedCreditAsInput(MinedCreditMessage msg)
    {
        CreditBatch batch = credit_system->ReconstructBatch(msg);

        CreditInBatch credit_in_batch = batch.GetCreditInBatch(batch.credits[0]);

        UnsignedTransaction rawtx;
        rawtx.inputs.push_back(credit_in_batch);
        return SignTransaction(rawtx, keydata);
    }

    SignedTransaction GetDoubleSpendTransactionUsingMinedCreditAsInput(MinedCreditMessage msg)
    {
        auto tx = GetTransactionUsingMinedCreditAsInput(msg);
        Credit output(GetNextPubKey().getvch(), ONE_CREDIT);
        tx.rawtx.outputs.push_back(output);
        return SignTransaction(tx.rawtx, keydata);
    }

    virtual void SetUp()
    {
        ACreditMessageHandlerWithTwoPeers::SetUp();

        MinedCreditMessage tip;
        tip.mined_credit.network_state.network_id = 1;

        for (int i = 0; i < 10; i++)
        {
            tip = NextMinedCreditMessage(tip);
            credit_message_handler->HandleValidMinedCreditMessage(tip);
            msgs.push_back(tip);
        }
        first_transaction = GetTransactionUsingMinedCreditAsInput(msgs[3]);
        credit_message_handler->AcceptTransaction(first_transaction);

        second_transaction = GetTransactionUsingMinedCreditAsInput(msgs[4]);
        credit_message_handler->AcceptTransaction(second_transaction);

        double_spend_of_first_transaction = GetDoubleSpendTransactionUsingMinedCreditAsInput(msgs[3]);
        credit_system->StoreTransaction(double_spend_of_first_transaction);

        message_containing_one_transaction = GetMinedCreditMessageContainingOneTransaction(first_transaction,
                                                                                           msgs.back());
        message_containing_double_spend = GetMinedCreditMessageContainingOneTransaction(double_spend_of_first_transaction,
                                                                                        msgs.back());
    }

    void AddSequenceOfMinedCreditMessagesToTip(uint64_t length)
    {
        MinedCreditMessage tip = credit_message_handler->Tip();

        for (int i = 0; i < length; i++)
        {
            tip = NextMinedCreditMessage(tip);
            credit_message_handler->HandleValidMinedCreditMessage(tip);
            msgs.push_back(tip);
        }
    }

    MinedCreditMessage GetMinedCreditMessageContainingOneTransaction(SignedTransaction tx, MinedCreditMessage prev_msg)
    {
        auto next_tip = NextMinedCreditMessage(prev_msg);
        next_tip.hash_list.full_hashes.push_back(tx.GetHash160());
        next_tip.hash_list.GenerateShortHashes();
        credit_system->SetBatchRootAndSizeAndMessageListHashAndSpentChainHash(next_tip);
        credit_system->StoreMinedCreditMessage(next_tip);
        return next_tip;
    }

    void HandleMessageAndAddNMoreAfterIt(MinedCreditMessage& msg, uint64_t N)
    {
        credit_message_handler->HandleValidMinedCreditMessage(msg);
        auto succeeding_msg = NextMinedCreditMessage(msg);
        for (int i = 0; i < N; i++)
        {
            credit_message_handler->HandleValidMinedCreditMessage(succeeding_msg);
            succeeding_msg = NextMinedCreditMessage(succeeding_msg);
        }
    }

    virtual void TearDown()
    {
        ACreditMessageHandlerWithTwoPeers::TearDown();
    }
};

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, HasTheCorrectTip)
{
    uint160 tip = calendar.LastMinedCreditMessageHash();
    ASSERT_THAT(tip, Eq(msgs.back().GetHash160()));
}

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, RetainsTheTransactionsNotIncludedInANewBatchAddedToTheTip)
{
    credit_message_handler->HandleValidMinedCreditMessage(message_containing_one_transaction);

    ASSERT_FALSE(VectorContainsEntry(credit_message_handler->builder->accepted_messages, first_transaction.GetHash160()));
    ASSERT_TRUE(VectorContainsEntry(credit_message_handler->builder->accepted_messages, second_transaction.GetHash160()));
}

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, RetainsTransactionsWhenSwitchingAcrossAFork)
{
    AddSequenceOfMinedCreditMessagesToTip(2);
    HandleMessageAndAddNMoreAfterIt(message_containing_one_transaction, 3);

    ASSERT_FALSE(VectorContainsEntry(credit_message_handler->builder->accepted_messages, first_transaction.GetHash160()));
    ASSERT_TRUE(VectorContainsEntry(credit_message_handler->builder->accepted_messages, second_transaction.GetHash160()));
}

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, UpdatesTheWalletWhenSwitchingAcrossAFork)
{
    MinedCreditMessage fork = credit_message_handler->Tip();
    uint64_t balance_at_fork = wallet->Balance();

    AddSequenceOfMinedCreditMessagesToTip(2);
    uint64_t balance_at_tip_of_first_branch = wallet->Balance();
    HandleMessageAndAddNMoreAfterIt(fork, 3);
    uint64_t balance_at_tip_of_second_branch = wallet->Balance();

    ASSERT_THAT(balance_at_tip_of_first_branch, Eq(balance_at_fork + 2 * ONE_CREDIT));
    ASSERT_THAT(balance_at_tip_of_second_branch, Eq(balance_at_fork + 3 * ONE_CREDIT));
}

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, DropsTransactionsWithDoubleSpendsWhenSwitchingAcrossAFork)
{
    HandleMessageAndAddNMoreAfterIt(message_containing_one_transaction, 3);
    HandleMessageAndAddNMoreAfterIt(message_containing_double_spend, 4);

    ASSERT_FALSE(VectorContainsEntry(credit_message_handler->builder->accepted_messages, first_transaction.GetHash160()));
    ASSERT_TRUE(VectorContainsEntry(credit_message_handler->builder->accepted_messages, second_transaction.GetHash160()));
}

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, GeneratesAMinedCreditMessageWithoutAProofOfWork)
{
    MinedCreditMessage msg = credit_message_handler->builder->GenerateMinedCreditMessageWithoutProofOfWork();
    msg.hash_list.RecoverFullHashes(msgdata);
    ASSERT_TRUE(VectorContainsEntry(msg.hash_list.full_hashes, first_transaction.GetHash160()));
    ASSERT_TRUE(VectorContainsEntry(msg.hash_list.full_hashes, second_transaction.GetHash160()));
}

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, GeneratesAValidMinedCreditMessageWithoutAProofOfWork)
{
    MinedCreditMessage msg = credit_message_handler->builder->GenerateMinedCreditMessageWithoutProofOfWork();
    creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    creditdata[msg.mined_credit.network_state.previous_mined_credit_message_hash]["passed_verification"] = true;
    ASSERT_TRUE(credit_message_handler->MinedCreditMessagePassesVerification(msg));
}

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, GeneratesAChain)
{
    MinedCreditMessage msg;

    for (int i = 0; i < 10; i++)
    {
        msg = credit_message_handler->builder->GenerateMinedCreditMessageWithoutProofOfWork();
        creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
        credit_system->StoreMinedCreditMessage(msg);
        credit_message_handler->HandleValidMinedCreditMessage(msg);
    }

    ASSERT_THAT(credit_message_handler->Tip(), Eq(msg));
}

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, RejectsDoubleSpendsWhichConflictWithTheAcceptedTransactions)
{
    credit_message_handler->HandleSignedTransaction(first_transaction);
    uint160 first_tx_hash = first_transaction.GetHash160();
    credit_message_handler->HandleSignedTransaction(double_spend_of_first_transaction);
    uint160 double_spend_hash = double_spend_of_first_transaction.GetHash160();
    ASSERT_THAT(VectorContainsEntry(credit_message_handler->builder->accepted_messages, first_tx_hash), Eq(true));
    ASSERT_THAT(VectorContainsEntry(credit_message_handler->builder->accepted_messages, double_spend_hash), Eq(false));
}

TEST_F(ACreditMessageHandlerWithAcceptedTransactions, RejectsDoubleSpendsThatConflictWithTheSpentChain)
{
    credit_message_handler->HandleSignedTransaction(first_transaction);
    uint160 first_tx_hash = first_transaction.GetHash160();

    auto msg = credit_message_handler->builder->GenerateMinedCreditMessageWithoutProofOfWork();
    creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
    credit_system->StoreMinedCreditMessage(msg);
    credit_message_handler->HandleValidMinedCreditMessage(msg);

    credit_message_handler->HandleSignedTransaction(double_spend_of_first_transaction);
    uint160 double_spend_hash = double_spend_of_first_transaction.GetHash160();

    ASSERT_THAT(VectorContainsEntry(credit_message_handler->builder->accepted_messages, first_tx_hash), Eq(false));
    ASSERT_THAT(VectorContainsEntry(credit_message_handler->builder->accepted_messages, double_spend_hash), Eq(false));
}