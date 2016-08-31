#ifndef FLEX_CREDITINBATCH_H
#define FLEX_CREDITINBATCH_H

#include "Credit.h"

#include "log.h"
#define LOG_CATEGORY "CreditInBatch.h"


class CreditInBatch : public Credit
{
public:
    uint64_t position;
    std::vector<uint160> branch;
    std::vector<uint160> diurn_branch;

    CreditInBatch() {}

    CreditInBatch(Credit credit, uint64_t position,
                  std::vector<uint160> branch);

    string_t ToString() const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(keydata);
        READWRITE(position);
        READWRITE(branch);
        READWRITE(diurn_branch);
    )

    JSON(amount, keydata, position, branch, diurn_branch);

    vch_t getvch();

    bool operator==(const CreditInBatch& other) const;

    bool operator<(const CreditInBatch& other) const;
};


#endif //FLEX_CREDITINBATCH_H
