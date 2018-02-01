#ifndef TELEPORT_CREDIT_H
#define TELEPORT_CREDIT_H

#include <src/crypto/point.h>
#include <src/base/base58.h>
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
    Point public_key{SECP256K1, 0};

    Credit() = default;

    Credit(Point public_key, uint64_t amount);

    explicit Credit(vch_t data);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(public_key);
    )

    std::string json()
    {
        std::string json_text("{");
        std::stringstream amount_stream;
        amount_stream << amount;
        json_text += "\"amount\": " + amount_stream.str();
        json_text += ",\"address\": \"" + GetTeleportAddressFromPublicKey(public_key) + "\"";
        json_text += "}";
        return json_text;
    }

    virtual string_t ToString();

    uint160 KeyHash() const;

    vch_t getvch() const;

    bool operator==(const Credit& other_credit) const;

    bool operator<(const Credit& other_credit) const;

    std::string TeleportAddress()
    {
        return GetTeleportAddressFromPublicKey(public_key);
    }

    static std::string GetTeleportAddressFromPublicKey(Point &pubkey)
    {
        uint8_t prefix{179}; // Teleport addresses start with T
        vch_t bytes = pubkey.getvch();
        bytes[0] = prefix; // overwrites byte which specifies curve (SECP256K1)
        auto address_52chars = EncodeBase58Check(bytes);

        // second letter is always a U, so remove it
        return "T" + std::string(address_52chars.begin() + 2, address_52chars.end());
    }
};


#endif //TELEPORT_CREDIT_H
