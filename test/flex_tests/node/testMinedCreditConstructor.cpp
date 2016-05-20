#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <src/crypto/point.h>
#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include "gmock/gmock.h"
#include "MinedCreditConstructor.h"

using namespace ::testing;
using namespace std;

class AMinedCreditConstructor : public Test
{
public:
    MemoryDataStore keydata;
    MemoryDataStore creditdata;
    MemoryDataStore msgdata;
    MinedCreditConstructor *mined_credit_constructor;

    virtual void SetUp()
    {
        mined_credit_constructor = new MinedCreditConstructor(keydata, creditdata, msgdata);
    }

    virtual void TearDown()
    {
        delete mined_credit_constructor;
    }
};

TEST_F(AMinedCreditConstructor, ConstructsAMinedCreditWithAnAmountOfOne)
{
    MinedCredit mined_credit = mined_credit_constructor->ConstructMinedCredit();
    ASSERT_THAT(mined_credit.amount, Eq(1));
}

TEST_F(AMinedCreditConstructor, ConstructsAMinedCreditWithAPublicKeyWhosePrivateKeyIsKnown)
{
    MinedCredit mined_credit = mined_credit_constructor->ConstructMinedCredit();
    Point public_key = mined_credit.PublicKey();
    CBigNum private_key = keydata[public_key]["privkey"];
    ASSERT_THAT(private_key, Ne(0));
    Point recovered_public_key(private_key);
    ASSERT_THAT(recovered_public_key, Eq(public_key));
}