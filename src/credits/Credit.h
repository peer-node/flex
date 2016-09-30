#ifndef TELEPORT_CREDIT_H
#define TELEPORT_CREDIT_H

#include "crypto/uint256.h"
#include "base/serialize.h"
#include "define.h"

#include "log.h"
#define LOG_CATEGORY "Credit.h"

#define ONE_CREDIT 100000000

class Credit
{
public:
    uint64_t amount{0};
    vch_t keydata;

    Credit(): amount(0) { }

    Credit(vch_t keydata, uint64_t amount);

    Credit(vch_t data);

    ~Credit() { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(keydata);
    )

    JSON(amount, keydata);

    virtual string_t ToString() const;

    uint160 KeyHash() const;

    vch_t getvch() const;

    bool operator==(const Credit& other_credit) const;

    bool operator<(const Credit& other_credit) const;
};


#endif //TELEPORT_CREDIT_H
