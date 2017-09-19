#include <boost/assign.hpp>
#include "CryptoCurrencyCommands.h"
#include "CurrencyCrypto.h"
#include "Currency.h"

using std::string;
using namespace boost::assign;

#include "log.h"
#define LOG_CATEGORY "CryptoCurrencyCommands.cpp"


StringMap default_commands
        = map_list_of ("getinfo",
                       "getinfo")
                ("getbalance",
                 "getbalance")
                ("broadcast",
                 "broadcast DATA")
                ("listunspent",
                 "listunspent")
                ("getnewaddress",
                 "getnewaddress")
                ("listaddresses",
                 "listaddresses")
                ("getaddressbalance",
                 "getaddressbalance ADDRESS")
                ("gettxout",
                 "gettxout TXID N")
                ("importaddress",
                 "importaddress ADDRESS")
                ("sendtoaddress",
                 "sendtoaddress ADDRESS AMOUNT")
                ("payto",
                 "payto ADDRESS AMOUNT")
                ("dumpprivkey",
                 "dumpprivkey ADDRESS")
                ("getprivatekeys",
                 "getprivatekeys ADDRESS")
                ("importprivkey",
                 "importprivkey PRIVKEY teleport false")
                ("sweepprivkey",
                 "sweepprivkey PRIVKEY 0")
                ("sweep",
                 "sweep PRIVKEY ADDRESS")
                ("walletpassphrase",
                 "walletpassphrase \"PASSPHRASE\" UNLOCK_TIME");

CryptoCurrencyCommands::CryptoCurrencyCommands(string currency_code, TeleportConfig &config):
        currency_code(currency_code), config(config)
{ }

void CryptoCurrencyCommands::SetCommands()
{
    log_ << "CryptoCurrencyCommands::SetCommands\n";
    for (auto item : default_commands)
    {
        string command_name = item.first;
        string command_string = item.second;
        log_ << command_name << ": " << command_string << "\n";

        if (supported_commands.count(command_name))
        {
            commands[command_name] = command_string;
        }
        else if (CurrencyInfo(command_name) != "")
        {
            commands[command_name] = CurrencyInfo(command_name);
            supported_commands.insert(command_name);
        }
    }
}

string CryptoCurrencyCommands::CurrencyInfo(string key)
{
    return config[currency_code + "-" + key];
}

string CryptoCurrencyCommands::ImportPrivateKeyCommand(CBigNum privkey)
{
    string secret = crypto->PrivKeyToCurrencySecret(privkey);
    string command = commands["importprivkey"];
    if (crypto->electrum)
    {
        command = Replace(command, " teleport false", "");
    }
    return Replace(command, "PRIVKEY", secret);
}

string CryptoCurrencyCommands::GetPrivateKeyCommand(string address)
{
    string command;
    log_ << "GetPrivateKeyCommand: " << address << "\n";

    if (supported_commands.count("dumpprivkey"))
    {
        log_ << "supports dumpprivkey!\n";
        command = commands["dumpprivkey"];
        log_ << "command = " << command << "\n";
    }
    else if (supported_commands.count("getprivatekeys"))
    {
        command = commands["getprivatekeys"];
    }
    else
    {
        log_ << "Neither dumpprivkey nor getprivatekeys are supported!\n";
    }
    return Replace(command, "ADDRESS", address);
}

string CryptoCurrencyCommands::SendToPubKeyCommand(Point pubkey, uint64_t amount)
{
    string address = crypto->PubKeyToCurrencyAddress(pubkey);
    string amount_string = crypto->currency->AmountToString(amount);
    string command = commands["sendtoaddress"];
    command = Replace(command, "ADDRESS", address);
    command = Replace(command, "AMOUNT", amount_string);
    return command;
}

string CryptoCurrencyCommands::ImportPubKeyCommand(Point pubkey)
{
    string address = crypto->PubKeyToCurrencyAddress(pubkey);
    string command = commands["importaddress"];
    return Replace(command, "ADDRESS", address);
}

string CryptoCurrencyCommands::GetPubKeyBalanceCommand(Point pubkey)
{
    string address = crypto->PubKeyToCurrencyAddress(pubkey);
    string command = commands["getaddressbalance"];
    return Replace(command, "ADDRESS", address);
}

string CryptoCurrencyCommands::ListUnspentCommand()
{
    return commands["listunspent"];
}

string CryptoCurrencyCommands::BroadcastCommand(string data)
{
    string command = commands["broadcast"];
    return Replace(command, "DATA", data);
}

string CryptoCurrencyCommands::GetNewAddressCommand()
{
    return commands["getnewaddress"];
}

string CryptoCurrencyCommands::ListAddressesCommand()
{
    return commands["listaddresses"];
}

string CryptoCurrencyCommands::HelpCommand()
{
    return commands["help"];
}

string CryptoCurrencyCommands::InfoCommand()
{
    return commands["getinfo"];
}

string CryptoCurrencyCommands::GetTxOutCommand(string txid, uint32_t n)
{
    string position = ToString(n);
    string command = commands["gettxout"];
    command = Replace(command, "TXID", txid);
    command = Replace(command, "N", position);
    return command;
}

string CryptoCurrencyCommands::UnlockWalletCommand(string password)
{
    string unlock_time = ToString(WALLET_UNLOCK_TIME);
    string command = commands["walletpassphrase"];
    command = Replace(command, "PASSPHRASE", password);
    command = Replace(command, "UNLOCK_TIME", unlock_time);
    return command;
}

bool CryptoCurrencyCommands::Supports(std::string command)
{
    return (bool) supported_commands.count(command);
}
