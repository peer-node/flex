// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef FLEX_CURRENCIES_H
#define FLEX_CURRENCIES_H


#include "crypto/point.h"
#include "flexnode/init.h"
#include "crypto/key.h"
#include "base/base58.h"


#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_writer_template.h"

#ifdef WIN32
#define POPEN _popen
#else
#define POPEN popen
#endif

#define WALLET_UNLOCK_TIME 60 // seconds

typedef map<string, string> StringMap;

std::string exec(char* cmd);

class Unspent
{
public:
    string txid;
    uint32_t n;
    string script_pubkey;
    string address;
    uint64_t amount;
    uint32_t confirmations;

    Unspent():
        amount(0)
    { }

    bool operator!()
    {
        return amount == 0;
    }
};

class CurrencyTxOut
{
public:
    string txid;
    uint32_t n;
    string address;
    uint64_t amount;
    uint32_t confirmations;

    CurrencyTxOut():
        amount(0)
    { }

    bool operator!()
    {
        return amount == 0;
    }
};

string_t AmountToString(uint64_t amount, uint32_t decimal_places);

string_t AmountToString(uint64_t amount, vch_t currency);

string_t CurrencyInfo(vch_t currency_code, string detail);

class CurrencyCommands
{
public:
    vch_t currency_code;

    CurrencyCommands() { }

    CurrencyCommands(vch_t currency_code);

    std::string BalanceCommand();

    std::string AddressBalanceCommand(string address);
    
    std::string GetAddressWithAmountAndProof(uint64_t amount);

    std::string SendToAddressCommand(string address, uint64_t amount);

    std::string SendFiatCommand(vch_t my_data, 
                                vch_t their_data,
                                uint64_t amount);

    std::string ValidateProposedFiatTransactionCommand(
        uint160 accept_commit_hash,
        vch_t payer_data,
        vch_t payee_data,
        uint64_t amount);
    
    std::string VerifyFiatPaymentCommand(vch_t payment_proof,
                                         vch_t payer_data,
                                         vch_t payee_data,
                                         uint64_t amount);
    
    string AmountToString(uint64_t amount);

    string AmountToString(uint64_t amount, uint32_t decimal_places);
};

void InitializeCryptoCurrencies();

class CurrencyCrypto;

class CryptoCurrencyCommands
{
public:
    vch_t currency_code;
    std::string executable;

    CryptoCurrencyCommands() { }

    CryptoCurrencyCommands(vch_t currency_code);

    void SetCommands();

    std::string BroadcastCommand(std::string data);

    std::string ImportPrivateKeyCommand(CBigNum privkey);

    std::string GetPrivateKeyCommand(string address);

    std::string SendToPubKeyCommand(Point pubkey, uint64_t amount);

    std::string ImportPubKeyCommand(Point pubkey);

    std::string GetPubKeyBalanceCommand(Point pubkey);

    std::string ListUnspentCommand();

    std::string GetNewAddressCommand();

    std::string ListAddressesCommand();

    std::string HelpCommand();

    std::string InfoCommand();

    std::string GetTxOutCommand(string txid, uint32_t n);

    std::string UnlockWalletCommand(string password);    
};


json_spirit::Array ConvertParameterTypes(const string &method, 
                            const vector<string> &params);

bool GetMethodAndParamsFromCommand(string& method,
                                   vector<string>& params,
                                   string command);

int64_t string_to_int(string str_num);


json_spirit::Object DoRPCCall(const string& host,
                              uint32_t port,
                              const string& authentication,
                              const string& method, 
                              const json_spirit::Array& params);

class CurrencyRPC
{
public:
    uint32_t port;
    string host;
    string user;
    string password;
    vch_t currency_code;

    CurrencyRPC():
        port(0)
    { }

    CurrencyRPC(vch_t currency_code):
        currency_code(currency_code)
    {
        port = string_to_int(CurrencyInfo(currency_code, "rpcport"));
        host = CurrencyInfo(currency_code, "rpchost");
        if (host == "")
            host = "127.0.0.1";

        user = CurrencyInfo(currency_code, "rpcuser");
        password = CurrencyInfo(currency_code, "rpcpassword");
    }

    json_spirit::Value ExecuteCommand(string command)
    {
        string method;
        vector<string> params;

        if (!GetMethodAndParamsFromCommand(method, params, command))
            return json_spirit::Value();

        json_spirit::Array converted_params = ConvertParameterTypes(method, 
                                                                    params);
        string authentication = user + ":" + password;

        json_spirit::Object reply = DoRPCCall(host, 
                                              port, 
                                              authentication,
                                              method, 
                                              converted_params);
        return find_value(reply, "result");
    }
};

class NotaryRPC
{
public:
    uint32_t port;
    string host;
    vch_t currency_code;

    NotaryRPC():
        port(0)
    { }

    NotaryRPC(vch_t currency_code):
        currency_code(currency_code)
    {
        port = string_to_int(CurrencyInfo(currency_code, "notaryrpcport"));
        host = CurrencyInfo(currency_code, "notaryrpchost");
        if (host == "")
            host = "127.0.0.1";
    }

    json_spirit::Value ExecuteCommand(string command)
    {
        string method;
        vector<string> params;

        if (!GetMethodAndParamsFromCommand(method, params, command))
            return json_spirit::Value();

        json_spirit::Array converted_params = ConvertParameterTypes(method, 
                                                                    params);
        
        json_spirit::Object reply = DoRPCCall(host, 
                                              port, 
                                              "", 
                                              method, 
                                              converted_params);
        return find_value(reply, "result");
    }
};

class Currency;

class CurrencyCrypto
{
public:  
    CryptoCurrencyCommands commands;
    vch_t privkey_prefix;
    vch_t pubkey_prefix;
    vch_t currency_code;
    CurrencyRPC rpc;
    uint8_t curve;
    bool compressed;
    bool usable;
    bool locked;

    CurrencyCrypto() { }

    CurrencyCrypto(vch_t currency_code);

    void Initialize();

    bool Supports(string_t command);

    void DetermineCurve();

    void DetermineCompressed();

    void DeterminePrefixes();

    void DetermineCapabilities();

    void DetermineUsable();

    std::string Broadcast(std::string data);

    std::string PubKeyToAddress(Point pubkey);
    
    std::string PrivKeyToSecret(CBigNum privkey);
    
    int64_t GetPubKeyBalance(Point pubkey);

    bool ImportPrivateKey(CBigNum privkey);

    bool ImportPrivateKey(string privkey_string);

    std::string DumpPrivKey(string address);

    std::string GetAnyDumpedPrivKey();

    std::string GetAnyAddress();

    std::string GetNewAddress();

    std::vector<std::string> ListAddresses();

    std::string MakeTradeable(uint64_t amount);

    CBigNum GetPrivateKey(string address);
    
    std::string AddNToTxId(string txid, Point pubkey, uint64_t amount);

    std::string SendToPubKey(Point pubkey, uint64_t amount);

    std::vector<Unspent> ListUnspent();

    Unspent GetUnspentWithAmount(uint64_t amount);

    CurrencyTxOut GetTxOut(string txid, uint32_t n);

    CurrencyTxOut GetTxOutWithAmount(uint64_t amount);

    bool VerifyPayment(string payment_proof,
                       Point pubkey, uint64_t amount, 
                       uint32_t confirmations_required);

    bool VerifyPayment(string txid, uint32_t n, 
                       Point pubkey, uint64_t amount,
                       uint32_t confirmations_required);

    bool IsLocked();
};



class Currency
{
public:
    vch_t currency_code;
    CurrencyCommands commands;
    bool is_cryptocurrency;
    CurrencyCrypto crypto;
    CurrencyRPC rpc;

    Currency() { }

    Currency(const Currency& other);
    
    Currency(vch_t currency_code, bool is_cryptocurrency);
    
    int64_t Balance();

    int64_t GetAddressBalance(string address);
    
    std::string GetNewAddress(string address);

    std::string SendToAddress(string address, uint64_t amount);

    std::string SendFiat(vch_t my_data, vch_t their_data, uint64_t amount);

    bool ValidateProposedFiatTransaction(uint160 accept_commit_hash,
                                         vch_t payer_data,
                                         vch_t payee_data,
                                         uint64_t amount);

    bool GetAddressAndProofOfFunds(string& address, 
                                   string& proof, 
                                   uint64_t amount);

    bool VerifyFiatPayment(vch_t payment_proof, 
                           vch_t payer_data,
                           vch_t payee_data,
                           uint64_t amount);
    
    int64_t RealToAmount(double amount_float, vch_t currency);

    int64_t RealToAmount(double amount_float);
};


#endif
