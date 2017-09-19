#include <src/base/util_format.h>
#include "Currency.h"
#include "NotaryRPC.h"
#include "string_functions.h"

using std::string;
using std::vector;

#include "log.h"
#define LOG_CATEGORY "Currency.cpp"


void Currency::Initialize()
{
    log_ << "Initializing " << currency_code << "\n";
    if (Info("fiat") == "")
    {
        is_cryptocurrency = true;
        crypto = new CurrencyCrypto(currency_code, config);
        crypto->SetCurrency(this);
        crypto->Initialize();
    }
}

int64_t Currency::Balance()
{
    Json::Value result;
    try
    {
        result = rpc.ExecuteCommand(commands.BalanceCommand());
    }
    catch (...)
    {
        return -1;
    }
    log_ << "Balance(): result is " << result.asString() << "\n";
    return ExtractBalanceFromString(result.asString());
}

int64_t Currency::GetAddressBalance(string address)
{
    string balance_command  = commands.AddressBalanceCommand(address);
    Json::Value result;
    try
    {
        result = rpc.ExecuteCommand(balance_command);
    }
    catch (...)
    { return -1; }

    return ExtractBalanceFromString(result.asString());
}

string Currency::SendToAddress(string currency_address, uint64_t amount)
{
    string command = commands.SendToAddressCommand(currency_address, amount);
    if (command == string(""))
        return string("");
    string result = rpc.ExecuteCommand(command).asString();
    log_ << "SendToAddress: result " << result << "\n";
    if (is_cryptocurrency)
    {
        if (crypto->Supports("payto") and not crypto->Supports("sendtoaddress"))
        {
            if (not Contains(result, "\"hex\":"))
            {
                log_ << "Tried to use payto, but got no hex\n";
                return result;
            }
            string before, after, hex;
            split_string(result, before, after, "\"hex\":");
            hex = GetStringBetweenQuotes(after);
            log_ << "broadcasting hex\n";
            return crypto->Broadcast(hex);
        }
    }
    return result;
}

bool Currency::GetAddressAndProofOfFunds(string& address, string& proof, uint64_t amount)
{
    if (is_cryptocurrency)
    {
        CurrencyTxOut txout = crypto->GetTxOutWithAmount(amount);
        if (!txout)
            return false;
        address = txout.address;
        proof = txout.txid + ":" + ToString(txout.n);
        return true;
    }
    string command = commands.GetAddressWithAmountAndProof(amount);
    if (command == "")
        return true;
    string output = rpc.ExecuteCommand(command).asString();
    split_string(output, address, proof);
    return true;
}

string Currency::SendFiat(vch_t my_data, vch_t their_data, uint64_t amount)
{
    string command = commands.SendFiatCommand(my_data, their_data, amount);
    if (command == "")
    {
        string payer(my_data.begin(), my_data.end());
        string payee(their_data.begin(), their_data.end());
        string url = Info("payment_url");
        if (url == "")
            return "";
        uint160 prompt_hash = GetTimeMicros();
//        guidata[prompt_hash]["payment_url"] = url;
//        guidata[prompt_hash]["payer"] = payer;
//        guidata[prompt_hash]["payee"] = payee;
//        guidata[prompt_hash].Location("events") = GetTimeMicros();
        string text_prompt("Please send "
                            + AmountToString(amount)
                            + " to " + payee + " from " + payer
                            + " by going to " + url + "\n");
        printf("%s\n", text_prompt.c_str());
        fprintf(stderr, "%s\n", text_prompt.c_str());
        log_ << text_prompt;
        return "";
    }
    return rpc.ExecuteCommand(command).asString();
}

bool Currency::ValidateProposedFiatTransaction(uint160 accept_commit_hash, vch_t payer_data, vch_t payee_data, uint64_t amount)
{
    string command = commands.ValidateProposedFiatTransactionCommand(accept_commit_hash,
                                                                     payer_data, payee_data, amount);
    if (command == string(""))
        return false;
    NotaryRPC notary_rpc(currency_code, config);
    string result = notary_rpc.ExecuteCommand(command).asString();
    log_ << "VerifyProposedFiatTransaction: result is " << result << "\n";
    return Replace(result, "\n", "") == "ok";
}

bool Currency::VerifyFiatPayment(vch_t payment_proof, vch_t payer_data, vch_t payee_data, uint64_t amount)
{
    if (is_cryptocurrency)
        return false;
    string command = commands.VerifyFiatPaymentCommand(payment_proof, payer_data, payee_data, amount);
    if (command == string(""))
        return false;
    NotaryRPC notary_rpc(currency_code, config);
    string result = notary_rpc.ExecuteCommand(command).asString();
    log_ << "VerifyFiatPayment: result is " << result << "\n";
    return Replace(result, "\n", "") == "ok";
}

int64_t Currency::RealToAmount(double amount_float)
{
    uint64_t conversion_factor = 1;

    for (uint32_t i = 0; i < decimal_places; i++)
        conversion_factor *= 10;

    return roundint64(amount_float * conversion_factor);
}

std::string Currency::AmountToString(uint64_t amount)
{
    char char_amount[50];
    if (decimal_places == 0)
    {
        sprintf(char_amount, "%lu", amount);
        return string(char_amount);
    }

    string string_amount = ToString(amount);

    while (string_amount.size() < decimal_places)
        string_amount = "0" + string_amount;

    uint64_t l = string_amount.size();
    string_amount = string_amount.substr(0, l - decimal_places)
                    + "." +
                    string_amount.substr(l - decimal_places, decimal_places);

    if (string_amount.substr(0, 1) == ".")
        string_amount = "0" + string_amount;

    return string_amount;
}

int64_t Currency::ExtractBalanceFromString(string input_string)
{
    size_t brace_location = input_string.find("{");
    string balance_string;
    if (brace_location == string::npos)
        balance_string = input_string;
    else
    {   // electrum says {"confirmed": "xxxxxx"}
        size_t location_of_confirmed = input_string.find("confirmed\": \"");
        if (location_of_confirmed == string::npos)
            return -1;
        size_t end_of_confirmed
                = input_string.find("\"", location_of_confirmed + 13);
        if (end_of_confirmed == string::npos)
            return -1;
        balance_string = input_string.substr(location_of_confirmed + 13,
                                             end_of_confirmed
                                             - (location_of_confirmed + 13));
    }
    log_ << "balance_string is " << balance_string << "\n";
    size_t dot_location = balance_string.find(".");
    log_ << "dot location is " << dot_location << "\n";
    if (dot_location == string::npos)
        balance_string = balance_string + ".";

    while (balance_string.length() <= balance_string.find(".") + decimal_places)
        balance_string = balance_string + "0";
    log_ << "balance_string is now " << balance_string << "\n";
    balance_string = balance_string.replace(balance_string.find("."), 1, "");
    log_ << "balance_string is now " << balance_string << "\n";
    while (balance_string.substr(0, 1) == "0")
        balance_string = balance_string.replace(balance_string.find("0"), 1, "");
    log_ << "returning " << string_to_uint(balance_string) << "\n";
    return string_to_uint(balance_string);
}