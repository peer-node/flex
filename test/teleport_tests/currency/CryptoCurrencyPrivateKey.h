#ifndef TELEPORT_CRYPTOCURRENCYPRIVATEKEY_H
#define TELEPORT_CRYPTOCURRENCYPRIVATEKEY_H


#include <src/crypto/bignum.h>
#include <src/crypto/key.h>
#include <src/base/base58.h>

#include "log.h"
#define LOG_CATEGORY "CryptoCurrencyPrivateKey"


class CryptoCurrencyPrivateKey
{
public:
    std::string currency_code;
    CBigNum private_key;

    CryptoCurrencyPrivateKey(std::string &currency_code, CBigNum &private_key):
            currency_code(currency_code), private_key(private_key)
    { }

    std::string ToString()
    {
        if (currency_code == "BTC")
            return PrivateKeyToCurrencySecret(private_key, 128, true);

        return private_key.ToString();
    }

    static CBigNum CurrencySecretToPrivateKey(std::string &currency_secret)
    {
        vch_t secret_bytes;
        CBigNum privkey;
        if (not DecodeBase58Check(currency_secret, secret_bytes))
        {
            log_ << "Failed to decode secret\n";
        }
        else
        {
            if (secret_bytes.size() == 34)
                secret_bytes.resize(33);
            reverse(secret_bytes.begin() + 1, secret_bytes.end());

            uint256 secret_256(vch_t(secret_bytes.begin() + 1, secret_bytes.end()));

            privkey = CBigNum(secret_256);
        }
        return privkey;
    }

    std::string PrivateKeyToCurrencySecret(CBigNum privkey, uint8_t prefix, bool compressed = true)
    {
        vch_t bytes{prefix};

        log_ << "PrivateKeyToCurrencySecret: prefix is " << bytes[0] << "\n";
        vch_t privkey_bytes = privkey.getvch32();

        reverse(privkey_bytes.begin(), privkey_bytes.end());

        log_ << "privkey bytes are: " << privkey_bytes << "\n";

        bytes.insert(bytes.end(), privkey_bytes.begin(), privkey_bytes.end());

        if (compressed)
        {
            log_ "adding final 1 before encoding\n";
            vch_t byte;
            byte.resize(1);
            byte[0] = 1;
            bytes.reserve(bytes.size() + 1);
            bytes.insert(bytes.end(), byte.begin(), byte.end());
        }

        log_ "bytes are now: " << bytes << "\n";

        return EncodeBase58Check(bytes);
    }
};


#endif //TELEPORT_CRYPTOCURRENCYPRIVATEKEY_H
