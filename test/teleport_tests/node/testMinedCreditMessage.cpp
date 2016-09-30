#include <test/teleport_tests/teleport_data/TestData.h>
#include <test/teleport_tests/mined_credits/MinedCredit.h>
#include <src/crypto/shorthash.h>
#include <src/credits/SignedTransaction.h>
#include "gmock/gmock.h"


#include "TeleportLocalServer.h"
#include "MinedCreditMessage.h"


using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class AMinedCreditMessage : public TestWithGlobalDatabases
{
public:
    MinedCreditMessage mined_credit_message;
};

TEST_F(AMinedCreditMessage, HasAMinedCredit)
{
    MinedCredit mined_credit = mined_credit_message.mined_credit;
}

TEST_F(AMinedCreditMessage, HasAShortHashList)
{
    ShortHashList<uint160> short_hash_list = mined_credit_message.hash_list;
}

TEST_F(AMinedCreditMessage, HasAProofOfWork)
{
    NetworkSpecificProofOfWork proof = mined_credit_message.proof_of_work;
}

TEST_F(AMinedCreditMessage, CanBeSerialized)
{
    creditdata["a"]["mined_credit_message"] = mined_credit_message;
}
