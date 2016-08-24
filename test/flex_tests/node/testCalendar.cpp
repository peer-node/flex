#define TEST_DIURN_SIZE 5
#define TEST_NUMBER_OF_CALENDS 10
#define TEST_DIURNAL_DIFFICULTY 29
#define TEST_DIFFICULTY 7

#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "CreditSystem.h"
#include "Calendar.h"

using namespace ::testing;
using namespace std;


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

TEST_F(ACalendar, ReturnsTheCalendCreditHashes)
{
    calendar.calends.push_back(Calend(msg));
    vector<uint160> calend_credit_hashes = calendar.GetCalendCreditHashes();
    ASSERT_THAT(calend_credit_hashes.size(), Eq(1));
    ASSERT_THAT(calend_credit_hashes[0], Eq(msg.mined_credit.GetHash160()));
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
    uint160 credit_hash = calendar.LastMinedCreditHash();
    ASSERT_THAT(credit_hash, Eq(msg.mined_credit.GetHash160()));
}

TEST_F(ACalendar, ReturnsTheHashOfTheLastCalendAsTheLastMinedCreditHashIfTheCurrentDiurnIsEmpty)
{
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(0));
    calendar.calends.push_back(Calend(msg));
    uint160 credit_hash = calendar.LastMinedCreditHash();
    ASSERT_THAT(credit_hash, Eq(msg.mined_credit.GetHash160()));
}

TEST_F(ACalendar, ReturnsZeroAsTheLastMinedCreditHashIfTheDiurnAndCalendsAreEmpty)
{
    uint160 credit_hash = calendar.LastMinedCreditHash();
    ASSERT_THAT(credit_hash, Eq(0));
}

TEST_F(ACalendar, ReturnsTheLastMinedCreditFromTheDiurn)
{
    calendar.current_diurn.Add(msg);
    MinedCredit mined_credit = calendar.LastMinedCredit();
    ASSERT_THAT(mined_credit, Eq(msg.mined_credit));
}

TEST_F(ACalendar, ReturnsTheLastMinedCreditFromTheCalends)
{
    calendar.calends.push_back(Calend(msg));
    MinedCredit mined_credit = calendar.LastMinedCredit();
    ASSERT_THAT(mined_credit, Eq(msg.mined_credit));
}

class ACalendarWithACreditSystem : public Test
{
public:
    Calendar calendar;
    MemoryDataStore msgdata, creditdata;
    CreditSystem *credit_system;

    virtual void SetUp()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
    }

    virtual void TearDown()
    {
        delete credit_system;
    }

    virtual EncodedNetworkState SucceedingNetworkState(MinedCredit mined_credit)
    {
        EncodedNetworkState next_state, previous_state = mined_credit.network_state;

        next_state.previous_mined_credit_hash = mined_credit.GetHash160();
        next_state.previous_diurn_root = previous_state.previous_diurn_root;
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
        next_msg.mined_credit.network_state = SucceedingNetworkState(msg.mined_credit);
        next_msg.hash_list.full_hashes.push_back(msg.mined_credit.GetHash160());
        next_msg.hash_list.GenerateShortHashes();
        credit_system->StoreMinedCreditMessage(next_msg);
        return next_msg;
    }

    uint160 CreditHashAtTipOfDiurn()
    {
        MinedCreditMessage starting_msg;
        starting_msg.mined_credit.network_state.diurnal_block_root = 2;
        starting_msg.mined_credit.network_state.diurnal_difficulty = TEST_DIURNAL_DIFFICULTY;
        credit_system->StoreMinedCreditMessage(starting_msg);
        auto next_msg = SucceedingMinedCreditMessage(starting_msg);
        for (int i = 0; i < 6; i++)
            next_msg = SucceedingMinedCreditMessage(next_msg);
        return next_msg.mined_credit.GetHash160();
    }
};

TEST_F(ACalendarWithACreditSystem, PopulatesTheCurrentDiurnBackToZeroWhenThereAreNoCalends)
{
    uint160 credit_hash = CreditHashAtTipOfDiurn();
    calendar.PopulateDiurn(credit_hash, credit_system);
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(8));
}

TEST_F(ACalendarWithACreditSystem, PopulatesTheCurrentDiurnBackToACalend)
{
    uint160 credit_hash = CreditHashAtTipOfDiurn();
    uint160 previous_hash = credit_system->PreviousCreditHash(credit_hash);
    uint160 antepenultimate_hash = credit_system->PreviousCreditHash(previous_hash);
    credit_system->creditdata[antepenultimate_hash]["is_calend"] = true;
    calendar.PopulateDiurn(credit_hash, credit_system);
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
        uint160 calend_credit_hash = calend_msg.mined_credit.GetHash160();
        creditdata[calend_credit_hash]["is_calend"] = true;
        credit_system->StoreMinedCreditMessage(calend_msg);
        return calend_msg;
    }

    MinedCreditMessage MsgFollowingCalend(MinedCreditMessage& calend_msg)
    {
        auto msg = SucceedingMinedCreditMessage(calend_msg);
        uint160 diurn_root = calend_msg.mined_credit.network_state.DiurnRoot();
        msg.mined_credit.network_state.previous_diurn_root = diurn_root;
        credit_system->StoreMinedCreditMessage(msg);
        return msg;
    }

    virtual uint160 CreditHashAtTipOfChainContainingCalends()
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
        return msg.mined_credit.GetHash160();
    }
};

TEST_F(ACalendarWithACreditSystemContainingCalends, PopulatesTheCalends)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar.PopulateCalends(credit_hash, credit_system);
    ASSERT_THAT(calendar.calends.size(), Eq(10));
}

TEST_F(ACalendarWithACreditSystemContainingCalends, PopulatesTheTopUpWork)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar.PopulateDiurn(credit_hash, credit_system);
    calendar.PopulateCalends(credit_hash, credit_system);
    calendar.PopulateTopUpWork(credit_hash, credit_system);

    uint160 total_calendar_work = calendar.TotalWork();
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
    uint160 total_work = mined_credit.network_state.previous_total_work + mined_credit.network_state.difficulty;

    bool enough_calendar_work = total_calendar_work >= total_work;
    ASSERT_THAT(enough_calendar_work, Eq(true));
}

TEST_F(ACalendarWithACreditSystemContainingCalends, CanBeConstructedUsingTheFinalMinedCreditHash)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
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

    virtual EncodedNetworkState SucceedingNetworkState(MinedCredit mined_credit)
    {
        auto next_state = credit_system->SucceedingNetworkState(mined_credit);
        auto prev_state = mined_credit.network_state;

        if (next_state.batch_number % 2 == 0)
            next_state.timestamp = prev_state.timestamp + 59 * 1000 * 1000;
        else
            next_state.timestamp = prev_state.timestamp + 61 * 1000 * 1000;

        next_state.difficulty = credit_system->GetNextDifficulty(mined_credit);
        next_state.diurnal_difficulty = credit_system->GetNextDiurnalDifficulty(mined_credit);

        return next_state;
    }

    virtual uint160 CreditHashAtTipOfChainContainingCalends()
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
        return msg.mined_credit.GetHash160();
    }
};

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheCalendDifficulties)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    bool ok = calendar.CheckCalendDifficulties(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheCurrentDiurnDifficulties)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    bool ok = calendar.CheckCurrentDiurnDifficulties();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheExtraWorkDifficulties)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    bool ok = calendar.CheckExtraWorkDifficulties();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksAllTheDifficulties)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    bool ok = calendar.CheckDifficulties(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheDiurnRoots)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    bool ok = calendar.CheckDiurnRoots();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheCreditHashesInTheCurrentDiurn)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    bool ok = calendar.CheckCreditHashesInCurrentDiurn();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheCreditHashesInTheExtraWork)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    bool ok = calendar.CheckCreditHashesInExtraWork();
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheDiurnRootsAndCreditHashes)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
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
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
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
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
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
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    MarkProofsInExtraWorkAsCheckedAndValid(calendar, creditdata);
    bool ok = calendar.CheckProofsOfWorkInExtraWork(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ChecksTheProofsOfWork)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    MarkProofsInCalendsAsCheckedAndValid(calendar, creditdata);
    MarkProofsInCurrentDiurnAsCheckedAndValid(calendar, creditdata);
    MarkProofsInExtraWorkAsCheckedAndValid(calendar, creditdata);
    bool ok = calendar.CheckProofsOfWork(credit_system);
    ASSERT_THAT(ok, Eq(true));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, ThrowsAnExceptionWhenTheWrongMinedCreditMessageIsAddedToTheTip)
{
    uint160 credit_hash = CreditHashAtTipOfChainContainingCalends();
    calendar = Calendar(credit_hash, credit_system);
    MinedCreditMessage message;
    message.mined_credit.network_state.previous_mined_credit_hash = 5;
    EXPECT_ANY_THROW(calendar.AddToTip(message, credit_system));
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, AddsAMinedCreditMessageToTheCurrentDiurn)
{
    calendar = Calendar(CreditHashAtTipOfChainContainingCalends(), credit_system);
    MinedCreditMessage msg;
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(calendar.LastMinedCredit());
    credit_system->StoreMinedCreditMessage(msg);
    calendar.AddToTip(msg, credit_system);
    ASSERT_THAT(calendar.current_diurn.Last(), Eq(msg.mined_credit.GetHash160()));
    ASSERT_THAT(calendar.current_diurn.Size(), Gt(1));
}

MinedCreditMessage NextCalend(Calendar& calendar, CreditSystem* credit_system)
{
    MinedCreditMessage msg;
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(calendar.LastMinedCredit());
    credit_system->StoreMinedCreditMessage(msg);
    uint160 credit_hash = msg.mined_credit.GetHash160();
    credit_system->creditdata[credit_hash]["is_calend"] = true;
    return msg;
}

TEST_F(ACalendarWithMinedCreditsWhoseDifficultiesVary, AddsACalendAndStartsANewDiurn)
{
    calendar = Calendar(CreditHashAtTipOfChainContainingCalends(), credit_system);
    auto msg = NextCalend(calendar, credit_system);
    uint64_t previous_number_of_calends = calendar.calends.size();
    calendar.AddToTip(msg, credit_system);
    ASSERT_THAT(calendar.calends.size(), Eq(previous_number_of_calends + 1));
    ASSERT_THAT(calendar.current_diurn.Last(), Eq(msg.mined_credit.GetHash160()));
    ASSERT_THAT(calendar.current_diurn.Size(), Eq(1));
}

TEST_F(ACalendarWithACreditSystemContainingCalends, PopulatesTheTopUpWorkWhenAddingACalend)
{
    calendar = Calendar(CreditHashAtTipOfChainContainingCalends(), credit_system);
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
    calendar = Calendar(CreditHashAtTipOfChainContainingCalends(), credit_system);
    uint160 previous_credit_hash = calendar.LastMinedCredit().network_state.previous_mined_credit_hash;
    calendar.RemoveLast(credit_system);
    ASSERT_THAT(calendar.LastMinedCreditHash(), Eq(previous_credit_hash));
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
    CreditBatch batch(network_state.previous_mined_credit_hash, network_state.batch_offset + network_state.batch_size);
    batch.Add(tx.rawtx.outputs[0]);
    network_state.batch_root = batch.Root();
    return batch.GetCreditInBatch(tx.rawtx.outputs[0]);
}

TEST_F(ACalendarWithACreditSystemContainingCalends, ChecksIfACreditInBatchFromTheCurrentDiurnIsValid)
{
    calendar = Calendar(CreditHashAtTipOfChainContainingCalends(), credit_system);
    auto tx = TransactionWithAnOutput(credit_system);
    auto next_network_state = credit_system->SucceedingNetworkState(calendar.LastMinedCredit());
    auto credit_in_batch = GetCreditInBatchFromTxOutputAndNetworkState(tx, next_network_state);
    auto msg = MinedCreditMessageContainingATransaction(tx, next_network_state, credit_system);
    calendar.AddToTip(msg, credit_system);
    bool ok = calendar.ValidateCreditInBatch(credit_in_batch);
    ASSERT_THAT(ok, Eq(true));
}

MinedCreditMessage GetNextMinedCreditMessage(Calendar& calendar, CreditSystem* credit_system)
{
    MinedCreditMessage msg, prev_msg = calendar.current_diurn.credits_in_diurn.back();
    msg.mined_credit.network_state = credit_system->SucceedingNetworkState(prev_msg.mined_credit);
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
    calendar = Calendar(CreditHashAtTipOfChainContainingCalends(), credit_system);
    auto tx = TransactionWithAnOutput(credit_system);
    auto next_network_state = credit_system->SucceedingNetworkState(calendar.LastMinedCredit());
    auto credit_in_batch = GetCreditInBatchFromTxOutputAndNetworkState(tx, next_network_state);
    auto msg = MinedCreditMessageContainingATransaction(tx, next_network_state, credit_system);
    calendar.AddToTip(msg, credit_system);

    ExtendCalendarWithMinedCreditMessages(calendar, 5, credit_system);

    auto calend = GetNextMinedCreditMessage(calendar, credit_system);
    creditdata[calend.mined_credit.GetHash160()]["is_calend"] = true;

    credit_in_batch.diurn_branch = calendar.current_diurn.Branch(msg.mined_credit.GetHash160());

    calendar.AddToTip(calend, credit_system);
    bool ok = calendar.ValidateCreditInBatch(credit_in_batch);
    ASSERT_THAT(ok, Eq(true));
}
