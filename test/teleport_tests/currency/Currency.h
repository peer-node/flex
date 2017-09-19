#ifndef TELEPORT_CURRENCY_H
#define TELEPORT_CURRENCY_H


#include "CurrencyCommands.h"
#include "CurrencyRPC.h"
#include "CurrencyCrypto.h"

#define DEFAULT_DECIMAL_PLACES 8


class Currency
{
public:
    std::string currency_code;
    CurrencyCommands commands;
    uint32_t decimal_places{DEFAULT_DECIMAL_PLACES};
    bool is_cryptocurrency{false};
    CurrencyCrypto *crypto{NULL};
    CurrencyRPC rpc;
    TeleportConfig &config;

    Currency(std::string currency_code, TeleportConfig &config):
            currency_code(currency_code), config(config), rpc(currency_code, config), commands(currency_code, config)
    {
        commands.SetCurrency(this);
    }

    ~Currency()
    {
        if (crypto != NULL)
        {
            delete crypto;
            crypto = NULL;
        }
    }

    void Initialize();

    std::string Info(std::string key)
    {
        return config[currency_code + "-" + key];
    }

    int64_t Balance();

    int64_t GetAddressBalance(std::string address);

    std::string SendToAddress(std::string address, uint64_t amount);

    std::string SendFiat(vch_t my_data, vch_t their_data, uint64_t amount);

    bool ValidateProposedFiatTransaction(uint160 accept_commit_hash,
                                         vch_t payer_data, vch_t payee_data, uint64_t amount);

    bool GetAddressAndProofOfFunds(std::string& address, std::string& proof, uint64_t amount);

    bool VerifyFiatPayment(vch_t payment_proof, vch_t payer_data, vch_t payee_data, uint64_t amount);

    std::string AmountToString(uint64_t amount);

    int64_t RealToAmount(double amount_float);

    int64_t ExtractBalanceFromString(std::string input_string);
};


#endif //TELEPORT_CURRENCY_H
