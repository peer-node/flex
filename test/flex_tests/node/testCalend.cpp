#include "gmock/gmock.h"
#include "CreditSystem.h"
#include "Calend.h"

using namespace ::testing;
using namespace std;

class ACalend : public Test
{
public:
    MinedCreditMessage msg;
    Calend calend;
};

TEST_F(ACalend, ReportsTheDiurnRoot)
{
    msg.mined_credit.network_state.previous_diurn_root = 1;
    msg.mined_credit.network_state.diurnal_block_root = 2;
    calend = Calend(msg);
    uint160 diurnal_root = calend.DiurnRoot();
    ASSERT_THAT(diurnal_root, Eq(SymmetricCombine(1, 2)));
}

TEST_F(ACalend, ReportsTheTotalCreditWork)
{
    msg.mined_credit.network_state.previous_total_work = 10;
    msg.mined_credit.network_state.difficulty = 5;
    calend = Calend(msg);
    uint160 total_work = calend.TotalCreditWork();
    ASSERT_THAT(total_work, Eq(15));
}
