#include <test/teleport_tests/currency/string_functions.h>
#include "CurrencyCommands.h"
#include "Currency.h"

using std::string;


CurrencyCommands::CurrencyCommands(string currency_code, TeleportConfig &config):
        currency_code(currency_code), config(config)
{
    command_strings = GetCurrencySettings();
}

StringMap CurrencyCommands::GetCurrencySettings()
{
    StringMap command_strings;
    for (auto item : config.settings)
    {
        auto key = item.first;
        auto value = item.second;
        if (Contains(key, currency_code + "-"))
        {
            key = Replace(key, currency_code + "-", "");
            command_strings[key] = value;
        }
    }
    return command_strings;
}

string CurrencyCommands::BalanceCommand()
{
    return command_strings["getbalance"];
}

string CurrencyCommands::AddressBalanceCommand(string address)
{
    string command = command_strings["getaddressbalance"];
    return Replace(command, "ADDRESS", address);
}

string CurrencyCommands::GetAddressWithAmountAndProof(uint64_t amount)
{
    string command = command_strings["getaddresswithamountandproof"];
    return Replace(command, "AMOUNT", currency->AmountToString(amount));
}

string CurrencyCommands::SendToAddressCommand(string address, uint64_t amount)
{
    string command;
    if (command_strings.count("sendtoaddress"))
    {
        command = command_strings["sendtoaddress"];
    }
    else if (command_strings.count("payto"))
    {
        command = command_strings["payto"];
    }
    command = Replace(command, "ADDRESS", address);
    command = Replace(command, "AMOUNT", currency->AmountToString(amount));
    return command;
}

string CurrencyCommands::SendFiatCommand(vch_t my_data, vch_t their_data, uint64_t amount)
{
    string command = command_strings["sendfiat"];
    string my_data_string(my_data.begin(), my_data.end());
    string their_data_string(their_data.begin(), their_data.end());
    command = Replace(command, "AMOUNT", currency->AmountToString(amount));
    command = Replace(command, "MY_DATA", my_data_string);
    command = Replace(command, "THEIR_DATA", their_data_string);
    return command;
}

string CurrencyCommands::ValidateProposedFiatTransactionCommand(uint160 accept_commit_hash,
                                                                vch_t payer_data, vch_t payee_data, uint64_t amount)
{
    string command = command_strings["notaryvalidate"];
    string payer_data_string(payer_data.begin(), payer_data.end());
    string payee_data_string(payee_data.begin(), payee_data.end());
    command = Replace(command, "ACCEPT_COMMIT_HASH", accept_commit_hash.ToString());
    command = Replace(command, "AMOUNT", currency->AmountToString(amount));
    command = Replace(command, "PAYER_DATA", payer_data_string);
    command = Replace(command, "PAYEE_DATA", payee_data_string);
    return command;
}

string CurrencyCommands::VerifyFiatPaymentCommand(vch_t payment_proof,
                                                  vch_t payer_data, vch_t payee_data, uint64_t amount)
{
    string command = command_strings["notaryverify"];
    command = Replace(command, "PAYMENT_PROOF", string(payment_proof.begin(), payment_proof.end()));
    command = Replace(command, "PAYER_DATA", string(payer_data.begin(), payer_data.end()));
    command = Replace(command, "PAYEE_DATA", string(payee_data.begin(), payee_data.end()));
    command = Replace(command, "AMOUNT", currency->AmountToString(amount));
    return command;
}
