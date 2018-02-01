#ifndef TELEPORT_CRYPTOCURRENCYADDRESS_H
#define TELEPORT_CRYPTOCURRENCYADDRESS_H


#include <src/crypto/point.h>
#include <src/base/BitcoinAddress.h>
#include <src/credits/creditsign.h>

class CryptoCurrencyAddress
{
public:
    std::string &currency_code;
    Point public_key;

    CryptoCurrencyAddress(std::string &currency_code, Point &public_key):
        currency_code(currency_code), public_key(public_key)
    { }

    std::string ToString()
    {
        if (currency_code == "BTC")
            return CBitcoinAddress(KeyHash(public_key)).ToString();

        if (currency_code == "TCR")
            return Credit::GetTeleportAddressFromPublicKey(public_key);

        return public_key.ToString();
    }
};


#endif //TELEPORT_CRYPTOCURRENCYADDRESS_H
