#include "Calendar.h"
#include "test/teleport_tests/node/credit/structures/CreditSystem.h"
#include <boost/range/adaptor/reversed.hpp>

#include "log.h"
#include "test/teleport_tests/node/historical_data/messages/CalendarFailureDetails.h"

#define LOG_CATEGORY "Calendar.cpp"


Calendar::Calendar() { }

Calendar::Calendar(uint160 msg_hash, CreditSystem *credit_system)
{
    PopulateCurrentDiurn(msg_hash, credit_system);
    PopulateCalends(msg_hash, credit_system);
    PopulateTopUpWork(msg_hash, credit_system);
}

uint160 Calendar::CalendWork()
{
    uint160 calend_work = 0;
    for (auto calend : calends)
        calend_work += calend.mined_credit.network_state.diurnal_difficulty;
    return calend_work;
}

std::vector<uint160> Calendar::GetCalendHashes()
{
    std::vector<uint160> calend_hashes;
    for (auto calend : calends)
        calend_hashes.push_back(calend.GetHash160());
    return calend_hashes;
}

bool Calendar::ContainsDiurn(uint160 diurn_root)
{
    for (auto calend : calends)
        if (calend.DiurnRoot() == diurn_root)
            return true;
    return false;
}

uint160 Calendar::LastMinedCreditMessageHash()
{
    if (current_diurn.Size() > 0)
        return current_diurn.credits_in_diurn.back().GetHash160();
    if (calends.size() > 0)
        return calends.back().GetHash160();
    return 0;
}

MinedCreditMessage Calendar::LastMinedCreditMessage()
{
    if (current_diurn.Size() > 0)
        return current_diurn.credits_in_diurn.back();
    if (calends.size() > 0)
        return calends.back();
    return MinedCreditMessage();
}

void Calendar::PopulateCurrentDiurn(uint160 msg_hash, CreditSystem *credit_system)
{
    current_diurn = credit_system->PopulateDiurn(msg_hash);
}

void Calendar::PopulateCalends(uint160 msg_hash, CreditSystem *credit_system)
{
    calends.resize(0);
    if (credit_system->IsCalend(msg_hash))
    {
        MinedCreditMessage msg = credit_system->msgdata[msg_hash]["msg"];
        calends.push_back(Calend(msg));
    }
    msg_hash = credit_system->PrecedingCalendHash(msg_hash);
    while (msg_hash != 0)
    {
        MinedCreditMessage msg = credit_system->msgdata[msg_hash]["msg"];
        calends.push_back(Calend(msg));

        msg_hash = credit_system->PrecedingCalendHash(msg_hash);
    }
    std::reverse(calends.begin(), calends.end());
}

void Calendar::PopulateTopUpWork(uint160 msg_hash, CreditSystem *credit_system)
{
    extra_work.resize(0);
    uint160 total_calendar_work = CalendWork() + current_diurn.Work();
    auto latest_network_state = LastMinedCreditMessage().mined_credit.network_state;
    uint160 total_credit_work = latest_network_state.previous_total_work + latest_network_state.difficulty;
    if (calends.size() > 0)
        total_calendar_work -= calends.back().mined_credit.network_state.difficulty;

    uint160 previous_credit_work_so_far = 0;
    for (auto calend : boost::adaptors::reverse(calends))
    {
        uint160 credit_work_so_far = calend.TotalCreditWork();
        uint160 credit_work_in_diurn = credit_work_so_far - previous_credit_work_so_far;
        AddTopUpWorkInDiurn(credit_work_in_diurn, total_credit_work, total_calendar_work, calend, credit_system);
        previous_credit_work_so_far = credit_work_so_far;
    }
    std::reverse(extra_work.begin(), extra_work.end());
}

void Calendar::AddTopUpWorkInDiurn(uint160 credit_work_in_diurn,
                                   uint160 total_credit_work,
                                   uint160& total_calendar_work,
                                   Calend& calend,
                                   CreditSystem* credit_system)
{
    std::vector<MinedCreditMessage> extra_work_in_diurn;
    uint160 calend_work = calend.mined_credit.network_state.diurnal_difficulty;
    EncodedNetworkState network_state = calend.mined_credit.network_state;

    uint160 msg_hash, remaining_credit_work_in_diurn = credit_work_in_diurn;

    MinedCreditMessage msg = (MinedCreditMessage)calend;

    while (remaining_credit_work_in_diurn > calend_work and total_credit_work > total_calendar_work)
    {
        network_state = msg.mined_credit.network_state;
        extra_work_in_diurn.push_back(msg);
        remaining_credit_work_in_diurn -= network_state.difficulty;
        total_calendar_work += network_state.difficulty;

        msg_hash = network_state.previous_mined_credit_message_hash;
        if (msg_hash == 0)
            break;
        msg = credit_system->msgdata[msg_hash]["msg"];
    }
    std::reverse(extra_work_in_diurn.begin(), extra_work_in_diurn.end());
    extra_work.push_back(extra_work_in_diurn);
}

uint160 Calendar::TopUpWork()
{
    uint160 top_up_work = 0;
    uint64_t diurn_number = 0;
    for (auto mined_credit_messages : extra_work)
    {
        for (auto msg : mined_credit_messages)
            top_up_work += msg.mined_credit.network_state.difficulty;
        diurn_number++;
    }
    return top_up_work;
}

uint160 Calendar::TotalWork()
{
    uint160 total_work = CalendWork();
    total_work += TopUpWork();
    total_work += current_diurn.Work();

    if (calends.size() > 0)
    {   // current diurn starts with last calend
        uint160 double_counted_work = calends.back().mined_credit.network_state.difficulty;
        total_work -= double_counted_work;
    }

    return total_work;
}

bool Calendar::CheckCreditHashesInExtraWork()
{
    if (extra_work.size() != calends.size())
        return false;

    for (uint32_t i = 0; i < extra_work.size(); i++)
    {
        if (not CheckCreditMessageHashesInSequenceOfConsecutiveMinedCreditMessages(extra_work[i]))
        {
            return false;
        }
        if (extra_work[i].size() > 0 and extra_work[i].back().mined_credit != calends[i].mined_credit)
        {
            return false;
        }
    }
    return true;
}

bool Calendar::CheckCreditMessageHashesInSequenceOfConsecutiveMinedCreditMessages(std::vector<MinedCreditMessage> msgs)
{
    for (uint32_t i = 1 ; i < msgs.size() ; i++)
        if (msgs[i].mined_credit.network_state.previous_mined_credit_message_hash != msgs[i-1].GetHash160())
        {
            return false;
        }
    return true;
}

bool Calendar::CheckCreditMessageHashesInCurrentDiurn()
{
    if (calends.size() == 0)
    {
        auto first_network_state = current_diurn.credits_in_diurn[0].mined_credit.network_state;
        if (first_network_state.previous_mined_credit_message_hash != 0)
            return false;
    }
    return CheckCreditMessageHashesInSequenceOfConsecutiveMinedCreditMessages(current_diurn.credits_in_diurn);
}

bool Calendar::CheckDiurnRoots()
{
    Calend previous_calend;
    uint64_t calend_number = 0;
    for (auto calend : calends)
    {
        if (calend_number == 0 and calend.mined_credit.network_state.previous_diurn_root != 0)
            return false;
        else if (calend_number > 0)
        {
            auto previous_network_state = previous_calend.mined_credit.network_state;
            uint160 previous_diurn_root = SymmetricCombine(previous_network_state.previous_diurn_root,
                                                           previous_network_state.diurnal_block_root);
            if (previous_diurn_root != calend.mined_credit.network_state.previous_diurn_root)
                return false;
        }
        previous_calend = calend;
        calend_number++;
    }
    if (calends.size() > 0)
    {
        auto first_msg_of_diurn = current_diurn.credits_in_diurn[0];
        if (calends.back().GetHash160() != first_msg_of_diurn.GetHash160())
            return false;
    }
    return true;
}

bool Calendar::CheckCalendHashes()
{
    uint160 previous_calend_hash = 0;
    for (auto calend : calends)
    {
        if (calend.mined_credit.network_state.previous_calend_hash != previous_calend_hash)
            return false;
        previous_calend_hash = calend.GetHash160();
    }

    if (calends.size() > 0 and current_diurn.credits_in_diurn[0].GetHash160() != previous_calend_hash)
        return false;

    for (uint64_t i = 1; i < current_diurn.Size(); i++)
    {
        auto msg = current_diurn.credits_in_diurn[i];
        if (msg.mined_credit.network_state.previous_calend_hash != previous_calend_hash)
            return false;
    }
    return true;
}

bool Calendar::CheckDiurnRootsAndCreditHashes()
{
    return CheckDiurnRoots() and CheckCalendHashes() and
            CheckCreditMessageHashesInCurrentDiurn() and CheckCreditHashesInExtraWork();
}

bool Calendar::CheckExtraWorkDifficultiesForDiurn(Calend calend, std::vector<MinedCreditMessage> msgs)
{
    if (not CheckDifficultiesOfConsecutiveSequenceOfMinedCreditMessages(msgs))
        return false;
    return msgs.size() == 0 or msgs.back().GetHash160() == calend.GetHash160();
}

bool Calendar::CheckExtraWorkDifficulties()
{
    if (calends.size() != extra_work.size())
        return false;
    for (uint32_t i = 0; i < calends.size(); i++)
        if (not CheckExtraWorkDifficultiesForDiurn(calends[i], extra_work[i]))
            return false;
    return true;
}

bool Calendar::CheckDifficultiesOfConsecutiveSequenceOfMinedCreditMessages(std::vector<MinedCreditMessage> msgs)
{
    uint64_t message_number = 0, previous_timestamp = 0, preceding_timestamp = 0;
    uint160 previous_difficulty = 0;
    for (auto msg : msgs)
    {
        uint160 difficulty = msg.mined_credit.network_state.difficulty;
        if (message_number > 1)
        {
            uint64_t batch_interval = previous_timestamp - preceding_timestamp;
            uint160 correct_difficulty = AdjustDifficultyAfterBatchInterval(previous_difficulty, batch_interval);
            if (difficulty != correct_difficulty)
                return false;
        }
        else if (message_number == 1)
        {
            uint160 one_possible_difficulty = AdjustDifficultyAfterBatchInterval(previous_difficulty, 0);
            uint160 other_possible_difficulty = AdjustDifficultyAfterBatchInterval(previous_difficulty, 61000000);
            if (difficulty != one_possible_difficulty and difficulty != other_possible_difficulty)
                return false;
        }
        previous_difficulty = difficulty;
        preceding_timestamp = previous_timestamp;
        previous_timestamp = msg.mined_credit.network_state.timestamp;
        message_number++;
    }
    return true;
}

bool Calendar::CheckCurrentDiurnDifficulties()
{
    return CheckDifficultiesOfConsecutiveSequenceOfMinedCreditMessages(current_diurn.credits_in_diurn);
}

bool Calendar::CheckCalendDifficulties(CreditSystem* credit_system)
{
    uint64_t calend_number = 0, previous_timestamp = 0, preceding_timestamp = 0;
    uint160 previous_difficulty = 0;

    for (auto calend : calends)
    {
        uint160 diurnal_difficulty = calend.mined_credit.network_state.diurnal_difficulty;

        if (calend_number < 2)
        {
            if (diurnal_difficulty != credit_system->initial_diurnal_difficulty)
                return false;
        }
        else
        {
            uint64_t diurn_duration = previous_timestamp - preceding_timestamp;
            uint160 correct_difficulty = AdjustDiurnalDifficultyAfterDiurnDuration(previous_difficulty, diurn_duration);
            if (diurnal_difficulty != correct_difficulty)
                return false;
        }
        previous_difficulty = diurnal_difficulty;
        preceding_timestamp = previous_timestamp;
        previous_timestamp = calend.mined_credit.network_state.timestamp;
        calend_number++;
    }
    return true;
}

bool Calendar::CheckDifficulties(CreditSystem* credit_system)
{
    if (calends.size() == 0 and current_diurn.Size() > 0)
    {
        MinedCredit first_mined_credit = current_diurn.credits_in_diurn.front().mined_credit;
        EncodedNetworkState first_network_state = first_mined_credit.network_state;
        if (first_network_state.difficulty != credit_system->initial_difficulty)
            return false;
    }
    return CheckCalendDifficulties(credit_system) and CheckCurrentDiurnDifficulties()
                                                  and CheckExtraWorkDifficulties();
}

bool Calendar::CheckRootsAndDifficulties()
{
    return CheckDiurnRootsAndCreditHashes() ;
}


bool Calendar::CheckRootsAndDifficulties(CreditSystem* credit_system)
{
    return CheckDiurnRootsAndCreditHashes() and CheckDifficulties(credit_system);
}

bool Calendar::CheckProofsOfWorkInCurrentDiurn(CreditSystem* credit_system)
{
    for (auto msg : current_diurn.credits_in_diurn)
    {
        if (not credit_system->QuickCheckProofOfWorkInMinedCreditMessage(msg))
            return false;
    }
    return true;
}

bool Calendar::CheckProofsOfWorkInCalends(CreditSystem *credit_system)
{
    for (auto calend : calends)
        if (not credit_system->QuickCheckProofOfWorkInCalend(calend))
            return false;
    return true;
}

bool Calendar::CheckProofsOfWorkInExtraWork(CreditSystem *credit_system)
{
    for (auto msgs : extra_work)
        for (auto msg : msgs)
            if (not credit_system->QuickCheckProofOfWorkInMinedCreditMessage(msg))
                return false;
    return true;
}

bool Calendar::CheckProofsOfWork(CreditSystem *credit_system)
{
    bool diurns_ok = CheckProofsOfWorkInCurrentDiurn(credit_system);
    bool calends_ok = CheckProofsOfWorkInCalends(credit_system);
    bool extra_work_ok = CheckProofsOfWorkInExtraWork(credit_system);

    return calends_ok and diurns_ok and extra_work_ok;
}

void Calendar::AddToTip(MinedCreditMessage &msg, CreditSystem* credit_system)
{
    if (msg.mined_credit.network_state.previous_mined_credit_message_hash != LastMinedCreditMessageHash())
        throw(std::runtime_error("Calendar: Attempt to add wrong mined credit to tip"));
    if (not credit_system->IsCalend(msg.GetHash160()))
    {
        current_diurn.Add(msg);
        return;
    }
    else
    {
        FinishCurrentDiurnAndStartNext(msg, credit_system);
    }
}

void Calendar::FinishCurrentDiurnAndStartNext(MinedCreditMessage &msg, CreditSystem *credit_system)
{
    calends.push_back(msg);
    uint160 diurn_root = calends.back().DiurnRoot();

    credit_system->RecordEndOfDiurn(calends.back(), current_diurn);

    current_diurn = Diurn();
    current_diurn.previous_diurn_root = diurn_root;
    current_diurn.Add(msg);
    PopulateTopUpWork(msg.mined_credit.GetHash160(), credit_system);
}

void Calendar::RemoveLast(CreditSystem* credit_system)
{
    if (current_diurn.Size() == 0)
            return;
    auto last_msg = LastMinedCreditMessage();
    *this = Calendar(last_msg.mined_credit.network_state.previous_mined_credit_message_hash, credit_system);
}

bool Calendar::ValidateCreditInBatchUsingCurrentDiurn(CreditInBatch credit_in_batch)
{
    if (credit_in_batch.branch.size() == 0)
        return false;
    uint160 batch_root = credit_in_batch.branch.back();
    bool ok = VerifyBranchFromOrderedHashTree((uint32_t) credit_in_batch.position, ((Credit)credit_in_batch).getvch(),
                                              credit_in_batch.branch, batch_root);
    if (not ok)
        return false;

    for (auto msg : current_diurn.credits_in_diurn)
        if (msg.mined_credit.network_state.batch_root == batch_root)
            return true;

    return false;
}

bool Calendar::ValidateCreditInBatchUsingPreviousDiurn(CreditInBatch credit_in_batch, std::vector<uint160> long_branch)
{
    uint160 diurn_root = long_branch.back();

    bool ok = VerifyBranchFromOrderedHashTree((uint32_t) credit_in_batch.position, ((Credit)credit_in_batch).getvch(),
                                              long_branch, diurn_root);

    log_ << "VerifyBranchFromOrderedHashTree: " << ok << "\n";
    if (not ok)
        return false;

    log_ << "ContainsDiurn: " << ContainsDiurn(diurn_root) << "\n";
    return ContainsDiurn(diurn_root);
}

bool Calendar::ValidateCreditInBatch(CreditInBatch credit_in_batch)
{
    if (credit_in_batch.diurn_branch.size() == 0)
    {
        log_ << "ValidateCreditInBatchUsingCurrentDiurn: " << ValidateCreditInBatchUsingCurrentDiurn(credit_in_batch) << "\n";
        return ValidateCreditInBatchUsingCurrentDiurn(credit_in_batch);
    }
    auto long_branch = credit_in_batch.branch;
    long_branch.pop_back();
    long_branch.insert(long_branch.end(), credit_in_batch.diurn_branch.begin(), credit_in_batch.diurn_branch.end());
    log_ << "ValidateCreditInBatchUsingPreviousDiurn: " << ValidateCreditInBatchUsingPreviousDiurn(credit_in_batch, long_branch) << "\n";
    return ValidateCreditInBatchUsingPreviousDiurn(credit_in_batch, long_branch);
}

bool Calendar::SpotCheckWork(CalendarFailureDetails &details)
{
    for (uint32_t i = 0; i < calends.size(); i++)
    {
        TwistWorkCheck check = calends[i].proof_of_work.proof.SpotCheck();

        if (!check.Valid())
        {
            uint160 diurn_root = calends[i].DiurnRoot();
            details.diurn_root = diurn_root;
            details.mined_credit_message_hash = calends[i].GetHash160();
            details.check = check;
            return false;
        }
    }
    return true;
}
