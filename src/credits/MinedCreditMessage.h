#ifndef FLEX_MINEDCREDITMESSAGE_H
#define FLEX_MINEDCREDITMESSAGE_H

#include <test/flex_tests/mined_credits/MinedCredit.h>
#include "crypto/hashtrees.h"
#include "crypto/shorthash.h"
#include "mining/work.h"
#include "Credit.h"

#include "log.h"
#define LOG_CATEGORY "MinedCreditMessage.h"


class MinedCreditMessage_
{
public:
    MinedCredit mined_credit;
    TwistWorkProof proof;
    ShortHashList<uint160> hash_list;
    uint64_t timestamp;

    MinedCreditMessage_(): timestamp(0) { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit);
        READWRITE(proof);
        READWRITE(hash_list);
        READWRITE(timestamp);
    )

    uint256 GetHash() const;

    uint160 GetHash160() const;

    bool operator==(const MinedCreditMessage_& other);

    bool HaveData();

    bool CheckHashList();

    string_t ToString() const;
};


#endif //FLEX_MINEDCREDITMESSAGE_H
