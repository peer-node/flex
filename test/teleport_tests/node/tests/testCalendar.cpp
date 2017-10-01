#define TEST_DIURN_SIZE 5
#define TEST_NUMBER_OF_CALENDS 10
#define TEST_DIURNAL_DIFFICULTY 29
#define TEST_DIFFICULTY 7

#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/credit/structures/CreditSystem.h"
#include "test/teleport_tests/node/calendar/Calendar.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"

class ACalendar : public Test
{
public:
    Calendar calendar;
    MinedCreditMessage msg;

    virtual void SetUp()
    {
        msg.mined_credit.network_state.diurnal_difficulty = INITIAL_DIURNAL_DIFFICULTY;
    }
};

TEST_F(ACalendar, ReportsTheTotalCalendWork)
{
    calendar.calends.push_back(Calend(msg));
    uint160 calend_work = calendar.CalendWork();
    ASSERT_THAT(calend_work, Eq(INITIAL_DIURNAL_DIFFICULTY));
}

TEST_F(ACalendar, ReturnsTheCalendHashes)
{
    calendar.calends.push_back(Calend(msg));
    vector<uint160> calend_hashes = calendar.GetCalendHashes();
    ASSERT_THAT(calend_hashes.size(), Eq(1));
    ASSERT_THAT(calend_hashes[0], Eq(msg.GetHash160()));
}

TEST_F(ACalendar, ReportsWhetherItContainsADiurn)
{
    msg.mined_credit.network_state.previous_diurn_root = 1;
    msg.mined_credit.network_state.diurnal_block_root = 2;

    uint160 diurn_root = SymmetricCombine(1, 2);
    calendar.calends.push_back(Calend(msg));

    bool calendar_contains_diurn = calendar.ContainsDiurn(diurn_root);
    ASSERT_THAT(calendar_contains_diurn, Eq(true));
}

TEST_F(ACalendar, ReturnsTheLastEntryInTheCurrentDiurnAsTheLastMinedCreditHashIfTheDiurnIsNonEmpty)
{
    calendar.current_diurn.Add(msg);
    uint160 credit_hash = calendar.LastMinedCreditMessageHash();
    ASSERT_THAT(credit_hash, Eq(msg.GetHash160()));
}

TEST_F(ACalendar, ReturnsTheHashOfTheLastCalendAsTheLastMinedCreditHashIfTheCurrentDiurnIsEmpty)
{
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(0));
    calendar.calends.push_back(Calend(msg));
    uint160 msg_hash = calendar.LastMinedCreditMessageHash();
    ASSERT_THAT(msg_hash, Eq(msg.GetHash160()));
}

TEST_F(ACalendar, ReturnsZeroAsTheLastMinedCreditHashIfTheDiurnAndCalendsAreEmpty)
{
    uint160 credit_hash = calendar.LastMinedCreditMessageHash();
    ASSERT_THAT(credit_hash, Eq(0));
}

TEST_F(ACalendar, ReturnsTheLastMinedCreditFromTheDiurn)
{
    calendar.current_diurn.Add(msg);
    MinedCreditMessage last_msg = calendar.LastMinedCreditMessage();
    ASSERT_THAT(last_msg, Eq(msg));
}

TEST_F(ACalendar, ReturnsTheLastMinedCreditFromTheCalends)
{
    calendar.calends.push_back(Calend(msg));
    MinedCreditMessage last_msg = calendar.LastMinedCreditMessage();
    ASSERT_THAT(last_msg, Eq(msg));
}

class ACalendarWithACreditSystem : public Test
{
public:
    Calendar calendar;
    MemoryDataStore msgdata, creditdata, keydata, depositdata;
    Data *data{NULL};
    CreditSystem *credit_system;

    virtual void SetUp()
    {
        data = new Data(msgdata, creditdata, keydata, depositdata);
        credit_system = new CreditSystem(*data);
    }

    virtual void TearDown()
    {
        delete credit_system;
        delete data;
    }

    virtual EncodedNetworkState SucceedingNetworkState(MinedCreditMessage msg)
    {
        EncodedNetworkState next_state, previous_state = msg.mined_credit.network_state;

        next_state.previous_mined_credit_message_hash = msg.GetHash160();
        next_state.previous_diurn_root = previous_state.previous_diurn_root;
        next_state.previous_calend_hash = previous_state.previous_calend_hash;
        next_state.batch_number = previous_state.batch_number + 1;
        if (next_state.batch_number % 2 == 0)
            next_state.timestamp = previous_state.timestamp + 59 * 1000 * 1000;
        else
            next_state.timestamp = previous_state.timestamp + 61 * 1000 * 1000;
        next_state.previous_total_work = previous_state.previous_total_work + previous_state.difficulty;
        next_state.diurnal_difficulty = TEST_DIURNAL_DIFFICULTY;
        next_state.difficulty = TEST_DIFFICULTY;

        return next_state;
    }

    MinedCreditMessage SucceedingMinedCreditMessage(MinedCreditMessage& msg)
    {
        MinedCreditMessage next_msg;
        next_msg.mined_credit.network_state = SucceedingNetworkState(msg);
        next_msg.hash_list.full_hashes.push_back(msg.GetHash160());
        next_msg.hash_list.GenerateShortHashes();
        credit_system->StoreMinedCreditMessage(next_msg);
        return next_msg;
    }

    uint160 HashOfMinedCreditMessageAtTipOfDiurn()
    {
        MinedCreditMessage starting_msg;
        starting_msg.mined_credit.network_state.diurnal_block_root = 2;
        starting_msg.mined_credit.network_state.diurnal_difficulty = TEST_DIURNAL_DIFFICULTY;
        credit_system->StoreMinedCreditMessage(starting_msg);
        auto next_msg = SucceedingMinedCreditMessage(starting_msg);
        for (int i = 0; i < 6; i++)
            next_msg = SucceedingMinedCreditMessage(next_msg);
        return next_msg.GetHash160();
    }
};

TEST_F(ACalendarWithACreditSystem, PopulatesTheCurrentDiurnBackToZeroWhenThereAreNoCalends)
{
    uint160 msg_hash = HashOfMinedCreditMessageAtTipOfDiurn();
    calendar.PopulateCurrentDiurn(msg_hash, credit_system);
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(8));
}

TEST_F(ACalendarWithACreditSystem, PopulatesTheCurrentDiurnBackToACalend)
{
    uint160 msg_hash = HashOfMinedCreditMessageAtTipOfDiurn();
    uint160 previous_hash = credit_system->PreviousMinedCreditMessageHash(msg_hash);
    uint160 antepenultimate_hash = credit_system->PreviousMinedCreditMessageHash(previous_hash);
    credit_system->creditdata[antepenultimate_hash]["is_calend"] = true;
    calendar.PopulateCurrentDiurn(msg_hash, credit_system);
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(3));
}


class ACalendarWithACreditSystemContainingCalends : public ACalendarWithACreditSystem
{
public:
    virtual void SetUp()
    {
        ACalendarWithACreditSystem::SetUp();
    }

    virtual void TearDown()
    {
        ACalendarWithACreditSystem::TearDown();
    }

    MinedCreditMessage TipOfChainStartingFrom(MinedCreditMessage& start_msg, uint32_t length)
    {
        auto msg = start_msg;
        for (int i = 0; i < length; i++)
            msg = SucceedingMinedCreditMessage(msg);
        return msg;
    }

    MinedCreditMessage CalendFollowingMsg(MinedCreditMessage& msg)
    {
        auto calend_msg = SucceedingMinedCreditMessage(msg);
        uint160 calend_hash = calend_msg.GetHash160();
        creditdata[calend_hash]["is_calend"] = true;
        credit_system->StoreMinedCreditMessage(calend_msg);
        return calend_msg;
    }

    MinedCreditMessage MsgFollowingCalend(MinedCreditMessage& calend_msg)
    {
        auto msg = SucceedingMinedCreditMessage(calend_msg);
        uint160 diurn_root = calend_msg.mined_credit.network_state.DiurnRoot();
        msg.mined_credit.network_state.previous_diurn_root = diurn_root;
        msg.mined_credit.network_state.previous_calend_hash = calend_msg.GetHash160();
        credit_system->StoreMinedCreditMessage(msg);
        return msg;
    }

    virtual uint160 MinedCreditMessageHashAtTipOfChainContainingCalends()
    {
        MinedCreditMessage starting_msg, msg;
        starting_msg.mined_credit.network_state.batch_number = 1;
        starting_msg.mined_credit.network_state.difficulty = TEST_DIFFICULTY;
        credit_system->StoreMinedCreditMessage(starting_msg);

        msg = starting_msg;

        for (int i = 0; i < TEST_NUMBER_OF_CALENDS; i++)
        {
            msg = TipOfChainStartingFrom(msg, TEST_DIURN_SIZE);
            msg = CalendFollowingMsg(msg);
            msg = MsgFollowingCalend(msg);
        }
        msg = TipOfChainStartingFrom(msg, TEST_DIURN_SIZE);
        return msg.GetHash160();
    }
};

TEST_F(ACalendarWithACreditSystemContainingCalends, PopulatesTheCalends)
{
    uint160 msg_hash = MinedCreditMessageHashAtTipOfChainContainingCalends();
    calendar.PopulateCalends(msg_hash, credit_system);
    ASSERT_THAT(calendar.calends.size(), Eq(10));
}

TEST_F(ACalendarWithACreditSystemContainingCalends, PopulatesTheTopUpWork)
{
    uint160 msg_hash = MinedCreditMessageHashAtTipOfChainContainingCalends();
    calendar.PopulateCurrentDiurn(msg_hash, credit_system);
    calendar.PopulateCalends(msg_hash, credit_system);
    calendar.PopulateTopUpWork(msg_hash, credit_system);

    uint160 total_calendar_work = calendar.TotalWork();
    MinedCreditMessage msg = msgdata[msg_hash]["msg"];
    uint160 total_work = msg.mined_credit.network_state.previous_total_work + msg.mined_credit.network_state.difficulty;

    bool enough_calendar_work = total_calendar_work >= total_work;
    ASSERT_THAT(enough_calendar_work, Eq(true));
}

TEST_F(ACalendarWithACreditSystemContainingCalends, CanBeConstructedUsingTheFinalMinedCreditHash)
{
    uint160 msg_hash = MinedCreditMessageHashAtTipOfChainContainingCalends();
    calendar = Calendar(msg_hash, credit_system);
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(TEST_DIURN_SIZE + 2));
    ASSERT_THAT(calendar.calends.size(), Eq(TEST_NUMBER_OF_CALENDS));
    ASSERT_THAT(calendar.extra_work.size(), Eq(TEST_NUMBER_OF_CALENDS));
}


class ACalendarWithMinedCreditsWhoseDifficultiesVary : public ACalendarWithACreditSystemContainingCalends
{
public:
    virtual void SetUp()
    {
        ACalendarWithACreditSystemContainingCalends::SetUp();
    }

    virtual void TearDown()
    {
        ACalendarWithACreditSystemContainingCalends::TearDown();
    }

    virtual EncodedNetworkState SucceedingNetworkState(MinedCreditMessage msg)
    {
        auto next_state = credit_system->SucceedingNetworkState(msg);
        auto prev_state = msg.mined_credit.network_state;

        if (next_state.batch_number % 2 == 0)
            next_state.timestamp = prev_state.timestamp + 59 * 1000 * 1000;
        else
            next_state.timestamp = prev_state.timestamp + 61 * 1000 * 1000;

        next_state.difficulty = credit_system->GetNextDifficulty(msg);
        next_state.diurnal_difficulty = credit_system->GetNextDiurnalDifficulty(msg);

        return next_state;
    }

    virtual uint160 MinedCreditMessageHashAtTipOfChainContainingCalends()
    {
        MinedCreditMessage starting_msg, msg;
        starting_msg.mined_credit.network_state.batch_number = 1;
        starting_msg.mined_credit.network_state.difficulty = credit_system->initial_difficulty;
        starting_msg.mined_credit.network_state.diurnal_difficulty = INITIAL_DIURNAL_DIFFICULTY;
        credit_system->StoreMinedCreditMessage(starting_msg);

        msg = starting_msg;

        for (int i = 0; i < TEST_NUMBER_OF_CALENDS; i++)
        {
            msg = TipOfChainStartingFrom(msg, TEST_DIURN_SIZE);
            msg = CalendFollowingMsg(msg);
            msg = MsgFollowingCalend(msg);
        }
        msg = TipOfChainStartingFrom(msg, TEST_DIURN_SIZE);
        return msg.GetHash160();
    }
};

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheCalendDifficulties)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    bool ok = calendar.CheckCalendDifficulties(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheCurrentDiurnDifficulties)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    bool ok = calendar.CheckCurrentDiurnDifficulties();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheExtraWorkDifficulties)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    bool ok = calendar.CheckExtraWorkDifficulties();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksAllTheDifficulties)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    bool ok = calendar.CheckDifficulties(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheDiurnRoots)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    bool ok = calendar.CheckDiurnRoots();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheCalendHashes)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    bool ok = calendar.CheckCalendHashes();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheCreditHashesInTheCurrentDiurn)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    bool ok = calendar.CheckCreditMessageHashesInCurrentDiurn();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheCreditHashesInTheExtraWork)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    bool ok = calendar.CheckCreditHashesInExtraWork();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheDiurnRootsAndCreditHashes)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    bool ok = calendar.CheckDiurnRootsAndCreditHashes();
    ASSERT_THAT(ok, Eq(true));
}

void MarkProofsInCurrentDiurnAsCheckedAndValid(Calendar &calendar, MemoryDataStore &creditdata)
{
    for (auto msg : calendar.current_diurn.credits_in_diurn)
        creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheProofsOfWorkInTheCurrentDiurn)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    // faster to mark proofs as checked than generate & check valid proofs
    MarkProofsInCurrentDiurnAsCheckedAndValid(calendar, creditdata);
    bool ok = calendar.CheckProofsOfWorkInCurrentDiurn(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

void MarkProofsInCalendsAsCheckedAndValid(Calendar &calendar, MemoryDataStore &creditdata)
{
    for (auto calend : calendar.calends)
        creditdata[calend.GetHash160()]["quickcalendcheck_ok"] = true;
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheProofsOfWorkInTheCalends)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    MarkProofsInCalendsAsCheckedAndValid(calendar, creditdata);
    bool ok = calendar.CheckProofsOfWorkInCalends(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

void MarkProofsInExtraWorkAsCheckedAndValid(Calendar &calendar, MemoryDataStore &creditdata)
{
    for (auto msgs : calendar.extra_work)
        for (auto msg : msgs)
            creditdata[msg.GetHash160()]["quickcheck_ok"] = true;
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheProofsOfWorkInTheExtraWork)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    MarkProofsInExtraWorkAsCheckedAndValid(calendar, creditdata);
    bool ok = calendar.CheckProofsOfWorkInExtraWork(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheProofsOfWork)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    MarkProofsInCalendsAsCheckedAndValid(calendar, creditdata);
    MarkProofsInCurrentDiurnAsCheckedAndValid(calendar, creditdata);
    MarkProofsInExtraWorkAsCheckedAndValid(calendar, creditdata);
    bool ok = calendar.CheckProofsOfWork(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ThrowsAnExceptionWhenTheWrongMinedCreditMessageIsAddedToTheTip)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    MinedCreditMessage message;
    message.mined_credit.network_state.previous_mined_credit_message_hash = 5;
    EXPECT_ANY_THROW(calendar.AddToTip(message, credit_system));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, AddsAMinedCreditMessageToTheCurrentDiurn)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    MinedCreditMessage msg;
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(calendar.LastMinedCreditMessage());
    credit_system->StoreMinedCreditMessage(msg);
    calendar.AddToTip(msg, credit_system);
    ASSERT_THAT(calendar.current_diurn.Last(), Eq(msg.mined_credit.GetHash160()));
    ASSERT_THAT(calendar.current_diurn.Size(), Gt(1));
}

MinedCreditMessage NextCalend(Calendar& calendar, CreditSystem* credit_system)
{
    MinedCreditMessage msg;
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(calendar.LastMinedCreditMessage());
    credit_system->StoreMinedCreditMessage(msg);
    uint160 msg_hash = msg.GetHash160();
    credit_system->creditdata[msg_hash]["is_calend"] = true;
    return msg;
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, AddsACalendAndStartsANewDiurn)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    auto msg = NextCalend(calendar, credit_system);
    uint64_t previous_number_of_calends = calendar.calends.size();
    calendar.AddToTip(msg, credit_system);
    ASSERT_THAT(calendar.calends.size(), Eq(previous_number_of_calends + 1));
    ASSERT_THAT(calendar.current_diurn.Last(), Eq(msg.mined_credit.GetHash160()));
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(1));
}

TEST_F(ACalendarWithACreditSystemContainingCalends, PopulatesTheTopUpWorkWhenAddingACalend)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    auto msg = NextCalend(calendar, credit_system);
    uint64_t previous_number_of_extra_work_vectors = calendar.extra_work.size();
    int64_t start = GetTimeMicros();
    calendar.AddToTip(msg, credit_system);
    uint64_t final_number_of_extra_work_vectors = calendar.extra_work.size();
    ASSERT_THAT(final_number_of_extra_work_vectors, Eq(previous_number_of_extra_work_vectors + 1));
    uint160 total_work = msg.mined_credit.network_state.previous_total_work + msg.mined_credit.network_state.difficulty;
    bool enough_calendar_work = calendar.TotalWork() >= total_work;
    ASSERT_THAT(enough_calendar_work, Eq(true));
}

TEST_F(ACalendarWithACreditSystemContainingCalends, RemovesTheLastMinedCreditFromTheTip)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    uint160 previous_msg_hash = calendar.LastMinedCreditMessage().mined_credit.network_state.previous_mined_credit_message_hash;
    calendar.RemoveLast(credit_system);
    ASSERT_THAT(calendar.LastMinedCreditMessageHash(), Eq(previous_msg_hash));
}

SignedTransaction TransactionWithAnOutput(CreditSystem* credit_system)
{
    SignedTransaction tx;
    Credit credit(Point(SECP256K1, 2), ONE_CREDIT);
    tx.rawtx.outputs.push_back(credit);
    credit_system->StoreTransaction(tx);
    return tx;
}

MinedCreditMessage MinedCreditMessageContainingATransaction(SignedTransaction tx,
                                                            EncodedNetworkState network_state,
                                                            CreditSystem* credit_system)
{
    MinedCreditMessage msg;
    msg.mined_credit.network_state = network_state;
    msg.hash_list.full_hashes.push_back(tx.GetHash160());
    msg.hash_list.GenerateShortHashes();
    credit_system->StoreMinedCreditMessage(msg);
    return msg;
}

CreditInBatch GetCreditInBatchFromTxOutputAndNetworkState(SignedTransaction tx,
                                                          EncodedNetworkState& network_state)
{
    CreditBatch batch(network_state.previous_mined_credit_message_hash,
                      network_state.batch_offset + network_state.batch_size);
    batch.Add(tx.rawtx.outputs[0]);
    network_state.batch_root = batch.Root();
    return batch.GetCreditInBatch(tx.rawtx.outputs[0]);
}

TEST_F(ACalendarWithACreditSystemContainingCalends, ChecksIfACreditInBatchFromTheCurrentDiurnIsValid)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    auto tx = TransactionWithAnOutput(credit_system);
    auto next_network_state = credit_system->SucceedingNetworkState(calendar.LastMinedCreditMessage());
    auto credit_in_batch = GetCreditInBatchFromTxOutputAndNetworkState(tx, next_network_state);
    auto msg = MinedCreditMessageContainingATransaction(tx, next_network_state, credit_system);
    calendar.AddToTip(msg, credit_system);
    bool ok = calendar.ValidateCreditInBatch(credit_in_batch);
    ASSERT_THAT(ok, Eq(true));
}

MinedCreditMessage GetNextMinedCreditMessage(Calendar& calendar, CreditSystem* credit_system)
{
    MinedCreditMessage msg, prev_msg = calendar.current_diurn.credits_in_diurn.back();
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(prev_msg);
    msg.mined_credit.network_state.diurnal_block_root = calendar.current_diurn.BlockRoot();
    credit_system->StoreMinedCreditMessage(msg);
    return msg;
}

void ExtendCalendarWithMinedCreditMessages(Calendar& calendar, uint64_t number_of_credits, CreditSystem* credit_system)
{
    for (int i = 0; i < number_of_credits; i++)
    {
        auto msg = GetNextMinedCreditMessage(calendar, credit_system);
        calendar.AddToTip(msg, credit_system);
    }
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksIfACreditInBatchFromAPreviousDiurnIsValid)
{
    calendar = Calendar(MinedCreditMessageHashAtTipOfChainContainingCalends(), credit_system);
    auto tx = TransactionWithAnOutput(credit_system);
    auto next_network_state = credit_system->SucceedingNetworkState(calendar.LastMinedCreditMessage());
    auto credit_in_batch = GetCreditInBatchFromTxOutputAndNetworkState(tx, next_network_state);
    auto msg = MinedCreditMessageContainingATransaction(tx, next_network_state, credit_system);
    calendar.AddToTip(msg, credit_system);

    ExtendCalendarWithMinedCreditMessages(calendar, 5, credit_system);

    auto calend = GetNextMinedCreditMessage(calendar, credit_system);
    creditdata[calend.GetHash160()]["is_calend"] = true;

    credit_in_batch.diurn_branch = calendar.current_diurn.Branch(msg.mined_credit.GetHash160());

    calendar.AddToTip(calend, credit_system);
    bool ok = calendar.ValidateCreditInBatch(credit_in_batch);
    ASSERT_THAT(ok, Eq(true));
}
