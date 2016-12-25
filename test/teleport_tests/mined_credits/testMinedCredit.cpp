#include "gmock/gmock.h"
#include "EncodedNetworkState.h"
#include "MinedCredit.h"

using namespace ::testing;
using namespace std;


class ANetworkState : public Test
{
public:
    EncodedNetworkState network_state;
};

TEST_F(ANetworkState, SpecifiesANetworkID)
{
    uint256 network_id = network_state.network_id;
}

TEST_F(ANetworkState, SpecifiesAPreviousMinedCreditHash)
{
    uint160 previous_mined_credit_hash = network_state.previous_mined_credit_hash;
}


class AMinedCredit : public TestWithGlobalDatabases
{
public:
    MinedCredit mined_credit;
};

TEST_F(AMinedCredit, SpecifiesANetworkState)
{
    EncodedNetworkState network_state = mined_credit.network_state;
}

TEST_F(AMinedCredit, CanBeStoredInADatabase)
{
    mined_credit.network_state.network_id = 1;
    creditdata["a"]["mined_credit"] = mined_credit;
    MinedCredit retrieved_mined_credit;
    retrieved_mined_credit = creditdata["a"]["mined_credit"];
    ASSERT_THAT(retrieved_mined_credit.network_state.network_id, Eq(1));
}

TEST_F(AMinedCredit, ProducesA20ByteHash)
{
    uint160 mined_credit_hash = mined_credit.GetHash160();
    MinedCredit other_mined_credit;
    other_mined_credit.network_state.network_id = 2;
    uint160 other_mined_credit_hash = other_mined_credit.GetHash160();
    ASSERT_THAT(mined_credit_hash, Ne(other_mined_credit_hash));
}

class AMinedCreditStoredInADatabase : public TestWithGlobalDatabases
{
public:
    MinedCredit mined_credit;
    uint160 mined_credit_hash;

    virtual void SetUp()
    {
        mined_credit.network_state.network_id = 1;
        mined_credit_hash = mined_credit.GetHash160();
        creditdata[mined_credit_hash]["mined_credit"] = mined_credit;
    }
};

TEST_F(AMinedCreditStoredInADatabase, CanBeRetrievedUsingItsHash)
{
    MinedCredit retrieved_credit;
    retrieved_credit = creditdata[mined_credit_hash]["mined_credit"];
    ASSERT_THAT(retrieved_credit.network_state.network_id, Eq(1));
}
