#ifndef FLEX_MINEDCREDITMESSAGE_H
#define FLEX_MINEDCREDITMESSAGE_H

#include "crypto/hashtrees.h"
#include "crypto/shorthash.h"
#include "mining/work.h"
#include "Credit.h"
#include "MinedCredit.h"

#include "log.h"
#define LOG_CATEGORY "MinedCreditMessage.h"


class MinedCreditMessage
{
public:
    MinedCredit mined_credit;
    TwistWorkProof proof;
    ShortHashList<uint160> hash_list;
    uint64_t timestamp;

    MinedCreditMessage(): timestamp(0) { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit);
        READWRITE(proof);
        READWRITE(hash_list);
        READWRITE(timestamp);
    )

    uint256 GetHash() const;

    uint160 GetHash160() const;

    bool operator==(const MinedCreditMessage& other);

    bool HaveData();

    bool CheckHashList();

    string_t ToString() const;
};


#endif //FLEX_MINEDCREDITMESSAGE_H
