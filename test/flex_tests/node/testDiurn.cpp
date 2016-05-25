#include "gmock/gmock.h"
#include "CreditSystem.h"
#include "Diurn.h"

using namespace ::testing;
using namespace std;

class ADiurn : public Test
{
public:
    Diurn diurn;
};

TEST_F(ADiurn, CanAddMinedCreditMessages)
{
    MinedCreditMessage msg;
    diurn.Add(msg);
    uint160 last_credit_hash = diurn.Last();
    ASSERT_THAT(last_credit_hash, Eq(msg.mined_credit.GetHash160()));
    ASSERT_THAT(diurn.credits_in_diurn.back(), Eq(msg));
    uint64_t diurn_size = diurn.Size();
    ASSERT_THAT(diurn_size, Eq(1));
}

TEST_F(ADiurn, ProvidesAValidBranchForAMinedCreditHash)
{
    MinedCreditMessage msg1, msg2;
    msg2.mined_credit.amount = 2;
    diurn.Add(msg1);
    diurn.Add(msg2);
    uint160 credit2_hash = msg2.mined_credit.GetHash160();
    vector<uint160> branch = diurn.Branch(credit2_hash);
    bool valid_branch = Diurn::VerifyBranch(credit2_hash, branch);
    ASSERT_THAT(valid_branch, Eq(true));
}

TEST_F(ADiurn, ReportsTheTotalWork)
{
    MinedCreditMessage msg1, msg2;
    msg1.mined_credit.network_state.difficulty = 1;
    msg2.mined_credit.network_state.difficulty = 2;
    diurn.Add(msg1);
    diurn.Add(msg2);
    uint160 total_work_in_diurn = diurn.Work();
    ASSERT_THAT(total_work_in_diurn, Eq(3));
}