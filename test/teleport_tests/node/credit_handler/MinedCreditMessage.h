#ifndef TELEPORT_MINEDCREDITMESSAGE_H
#define TELEPORT_MINEDCREDITMESSAGE_H


#include <test/teleport_tests/mined_credits/MinedCredit.h>
#include <src/crypto/shorthash.h>
#include <test/teleport_tests/mining/NetworkSpecificProofOfWork.h>

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

    JSON(mined_credit, hash_list, proof_of_work);

    DEPENDENCIES();

    HASH160();

    bool operator==(const MinedCreditMessage &other) const
    {
        return mined_credit == other.mined_credit and hash_list == other.hash_list
                                                  and proof_of_work == other.proof_of_work;
    }

    bool operator!=(const MinedCreditMessage &other) const
    {
        return not (*this == other);
    }
};


#endif //TELEPORT_MINEDCREDITMESSAGE_H
