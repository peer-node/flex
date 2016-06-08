#ifndef FLEX_MINEDCREDITMESSAGE_H
#define FLEX_MINEDCREDITMESSAGE_H


#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <src/crypto/shorthash.h>
#include <test/flex_tests/mining/NetworkSpecificProofOfWork.h>

class MinedCreditMessage
{
public:
    MinedCredit mined_credit;
    ShortHashList<uint160> hash_list;
    NetworkSpecificProofOfWork proof_of_work;

    MinedCreditMessage() { mined_credit.amount = ONE_CREDIT; }

    static std::string Type() { return "msg"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit);
        READWRITE(hash_list);
        READWRITE(proof_of_work);
    )

    DEPENDENCIES();

    uint160 GetHash160();

    bool operator==(const MinedCreditMessage &other) const
    {
        return mined_credit == other.mined_credit and hash_list == other.hash_list
                                                  and proof_of_work == other.proof_of_work;
    }
};


#endif //FLEX_MINEDCREDITMESSAGE_H
