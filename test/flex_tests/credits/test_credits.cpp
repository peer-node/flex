#include "gmock/gmock.h"
#include "../flex_data/TestData.h"
#include "credits/Credit.h"
#include "credits/MinedCredit.h"
#include "credits/CreditInBatch.h"
#include "credits/MinedCreditMessage.h"
#include "credits/UnsignedTransaction.h"
#include "credits/SignedTransaction.h"

using namespace ::testing;

class ACredit : public TestWithGlobalDatabases
{
public:
    Credit credit;
};

TEST_F(ACredit, HasZeroAmountByDefault)
{
    ASSERT_THAT(credit.amount, Eq(0));
}

TEST_F(ACredit, CanBeRetrievedFromTheDataBase)
{
    credit = creditdata["some"]["credit"];
    ASSERT_THAT(credit.amount, Eq(0));
}

TEST_F(ACredit, CanBeInsertedIntoTheDataBase)
{
    creditdata["some"]["credit"] = credit;
}

TEST_F(ACredit, RetainsItsDataWhenRetrievedFromTheDataBase)
{
    credit.amount = 100;
    creditdata["some"]["credit"] = credit;
    credit.amount = 0;
    credit = creditdata["some"]["credit"];
    ASSERT_THAT(credit.amount, Eq(100));
}


class AMinedCredit : public TestWithGlobalDatabases
{
public:
    MinedCredit mined_credit;
};

TEST_F(AMinedCredit, HasZeroAmountByDefault)
{
    ASSERT_THAT(mined_credit.amount, Eq(0));
}

TEST_F(AMinedCredit, HasZeroPreviousMinedCreditHashByDefault)
{
    ASSERT_THAT(mined_credit.previous_mined_credit_hash, Eq(0));
}

class ACreditInBatch : public TestWithGlobalDatabases
{
public:
    CreditInBatch credit_in_batch;
};

TEST_F(ACreditInBatch, HasZeroAmountByDefault)
{
    ASSERT_THAT(credit_in_batch.amount, Eq(0));
}

TEST_F(ACreditInBatch, RetainsDataWhenRetrievedFromDatabase)
{
    credit_in_batch.amount = 100;
    creditdata["batch1"]["entry1"] = credit_in_batch;
    CreditInBatch credit_in_batch2;
    credit_in_batch2 = creditdata["batch1"]["entry1"];
    ASSERT_THAT(credit_in_batch2.amount, Eq(100));
}


class AMinedCreditMessage : public TestWithGlobalDatabases
{
public:
    MinedCreditMessage mined_credit_message;
};

TEST_F(AMinedCreditMessage, HasZeroTimeStampByDefault)
{
    ASSERT_THAT(mined_credit_message.timestamp, Eq(0));
}


class AnUnsignedTransaction: public TestWithGlobalDatabases
{
public:
    UnsignedTransaction unsigned_transaction;
};

TEST_F(AnUnsignedTransaction, HasNoInputsByDefaults)
{
    ASSERT_THAT(unsigned_transaction.inputs.size(), Eq(0));
}

class ASignedTransaction: public TestWithGlobalDatabases
{
public:
    SignedTransaction signed_transaction;
};

TEST_F(ASignedTransaction, HasNoInputsByDefaults)
{
    ASSERT_THAT(signed_transaction.rawtx.inputs.size(), Eq(0));
}