#ifndef TELEPORT_CREDITBATCH_H
#define TELEPORT_CREDITBATCH_H

#include "crypto/hashtrees.h"
#include "Credit.h"
#include "CreditInBatch.h"

#include "log.h"
#define LOG_CATEGORY "CreditBatch.h"


class CreditBatch
{
public:
    uint160 previous_credit_hash;
    std::vector<Credit> credits;
    OrderedHashTree tree;
    std::vector<vch_t> serialized_credits;

    JSON(previous_credit_hash, credits);

    CreditBatch();

    CreditBatch(uint160 previous_credit_hash, uint64_t offset);

    CreditBatch(const CreditBatch& other);

    string_t ToString() const;

    bool Add(Credit credit);

    bool AddCredits(std::vector<Credit> credits_);

    uint160 Root();

    uint64_t size();

    int32_t Position(Credit credit);

    Credit GetCredit(uint32_t position);

    CreditInBatch GetCreditInBatch(Credit credit);

    std::vector<uint160> Branch(Credit credit);

    CreditBatch &operator=(const CreditBatch &other);
};



#endif //TELEPORT_CREDITBATCH_H
