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

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit);
        READWRITE(hash_list);
        READWRITE(proof_of_work);
    )
};


#endif //FLEX_MINEDCREDITMESSAGE_H
