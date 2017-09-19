#ifndef TELEPORT_CURRENCYCOMMANDS_H
#define TELEPORT_CURRENCYCOMMANDS_H

#include "define.h"
#include <test/teleport_tests/node/config/TeleportConfig.h>

class Currency;

class CurrencyCommands
{
public:
    std::string currency_code;
    TeleportConfig &config;
    std::map<std::string, std::string> command_strings;
    Currency *currency;

    CurrencyCommands(std::string currency_code, TeleportConfig &config_);

    void SetCurrency(Currency *currency_) { currency = currency_; }

    std::string BalanceCommand();

    std::string AddressBalanceCommand(std::string address);

    std::string GetAddressWithAmountAndProof(uint64_t amount);

    std::string SendToAddressCommand(std::string address, uint64_t amount);

    std::string SendFiatCommand(vch_t my_data, vch_t their_data, uint64_t amount);

    std::string ValidateProposedFiatTransactionCommand(uint160 accept_commit_hash,
                                                       vch_t payer_data, vch_t payee_data, uint64_t amount);

    std::string VerifyFiatPaymentCommand(vch_t payment_proof, vch_t payer_data, vch_t payee_data, uint64_t amount);

    std::map<std::string, std::string> GetCurrencySettings();

};

#endif //TELEPORT_CURRENCYCOMMANDS_H
