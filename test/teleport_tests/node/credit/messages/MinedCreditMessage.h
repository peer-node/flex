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

    static std::string Type() { return "msg"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit);
        READWRITE(hash_list);
        READWRITE(proof_of_work);
    )

    std::string json()
    {
        std::string json_text("{");
        json_text += "\"mined_credit\": " + mined_credit.json();
        json_text += ",\"hash_list\": " + hash_list.json();
        json_text += ",\"proof_of_work\": " + proof_of_work.json();
        json_text += ",\"mined_credit_message_hash\": \"" + GetHash160().ToString() + "\"";
        json_text += "}";
        return json_text;
    }

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
