#ifndef FLEX_UNSIGNEDTRANSACTION_H
#define FLEX_UNSIGNEDTRANSACTION_H

#include "Credit.h"
#include "crypto/point.h"
#include "MinedCredit.h"
#include "CreditInBatch.h"

#include "log.h"
#define LOG_CATEGORY "UnsignedTransaction.h"

class UnsignedTransaction
{
public:
    std::vector<CreditInBatch> inputs;
    std::vector<Credit> outputs;
    std::vector<Point> pubkeys;
    bool up_to_date;
    uint256 hash;

    UnsignedTransaction();

    string_t ToString() const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(inputs);
        READWRITE(outputs);
        READWRITE(pubkeys);
    )

    void AddInput(CreditInBatch batch_credit, bool is_keyhash);

    bool AddOutput(Credit credit);

    uint256 GetHash();

    bool operator!();
};



#endif //FLEX_UNSIGNEDTRANSACTION_H
