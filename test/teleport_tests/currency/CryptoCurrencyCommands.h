#ifndef TELEPORT_CRYPTOCURRENCYCOMMANDS_H
#define TELEPORT_CRYPTOCURRENCYCOMMANDS_H

#include "define.h"
#include "crypto/point.h"
#include <test/teleport_tests/node/config/TeleportConfig.h>
#include "string_functions.h"
#include "boost/assign.hpp"

#define WALLET_UNLOCK_TIME 60 // seconds

using namespace boost::assign;


extern StringMap default_commands;

class CurrencyCrypto;

class CryptoCurrencyCommands
{
public:
    std::string currency_code;
    std::string executable;
    TeleportConfig &config;
    StringMap commands;
    std::set<std::string> supported_commands;
    CurrencyCrypto *crypto{NULL};

    CryptoCurrencyCommands(std::string currency_code, TeleportConfig &config);

    void SetCurrencyCrypto(CurrencyCrypto *crypto_) { crypto = crypto_; }

    void SetCommands();

    bool Supports(std::string command);

    std::string BroadcastCommand(std::string data);

    std::string ImportPrivateKeyCommand(CBigNum privkey);

    std::string GetPrivateKeyCommand(std::string address);

    std::string SendToPubKeyCommand(Point pubkey, uint64_t amount);

    std::string ImportPubKeyCommand(Point pubkey);

    std::string GetPubKeyBalanceCommand(Point pubkey);

    std::string ListUnspentCommand();

    std::string GetNewAddressCommand();

    std::string ListAddressesCommand();

    std::string HelpCommand();

    std::string InfoCommand();

    std::string GetTxOutCommand(std::string txid, uint32_t n);

    std::string UnlockWalletCommand(std::string password);

    std::string CurrencyInfo(std::string key);
};


#endif //TELEPORT_CRYPTOCURRENCYCOMMANDS_H
