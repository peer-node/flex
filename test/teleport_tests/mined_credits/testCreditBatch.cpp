#include "gmock/gmock.h"
#include "credits/CreditBatch.h"

using namespace ::testing;
using namespace std;


TEST(ACreditBatch, CanAddCredits)
{
    CreditBatch credit_batch;
    Credit credit;
    credit.amount = 10;
    credit_batch.Add(credit);
    ASSERT_THAT(credit_batch.credits.size(), Eq(1));
}