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
    uint64_t amount{ONE_CREDIT};
    vch_t keydata;

    Credit() { }

    Credit(vch_t keydata, uint64_t amount);

    Credit(vch_t data);

    ~Credit() { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(keydata);
    )

    std::string json()
    {
        std::string json_text("{");
        std::stringstream ss;
        ss << amount;
        json_text += "\"amount\": " + ss.str();
        json_text += ",\"keyhash\": \"" + KeyHash().ToString() + "\"";
        json_text += "}";
        return json_text;
    }

    virtual string_t ToString() const;

    uint160 KeyHash() const;

    vch_t getvch() const;

    bool operator==(const Credit& other_credit) const;

    bool operator<(const Credit& other_credit) const;
};


#endif //TELEPORT_CREDIT_H
