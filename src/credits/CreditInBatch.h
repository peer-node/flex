#ifndef TELEPORT_CREDITINBATCH_H
#define TELEPORT_CREDITINBATCH_H

#include "Credit.h"

#include "log.h"
#define LOG_CATEGORY "CreditInBatch.h"


class CreditInBatch : public Credit
{
public:
    uint64_t position{0};
    std::vector<uint160> branch;
    std::vector<uint160> diurn_branch;

    CreditInBatch() = default;

    CreditInBatch(Credit credit, uint64_t position, std::vector<uint160> branch);

    string_t ToString() const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(public_key);
        READWRITE(position);
        READWRITE(branch);
        READWRITE(diurn_branch);
    )

    JSON(amount, public_key, position, branch, diurn_branch);

    vch_t getvch();

    bool operator==(const CreditInBatch& other) const;

    bool operator<(const CreditInBatch& other) const;
};


#endif //TELEPORT_CREDITINBATCH_H
