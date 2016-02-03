// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#include <boost/algorithm/string.hpp>
#include "json/json_spirit_reader.h"
#include "json/json_spirit.h"
#include "flexnode/flexnode.h"
#include "boost/assign.hpp"
#include "rpc/rpcprotocol.h"

#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include "log.h"
#define LOG_CATEGORY "currencies.cpp"

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace boost::assign;
using namespace json_spirit;


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
                   "importprivkey PRIVKEY flex false")
                  ("sweepprivkey",
                   "sweepprivkey PRIVKEY 0")
                  ("sweep",
                   "sweep PRIVKEY ADDRESS")
                  ("walletpassphrase",
                   "walletpassphrase \"PASSPHRASE\" UNLOCK_TIME");

int64_t string_to_int(string str_num)
{
    return strtoll(str_num.c_str(), NULL, 0);
}

uint64_t string_to_uint(string str_num)
{
    return strtoul(str_num.c_str(), NULL, 0);
}

string lowercase(string s)
{
    boost::algorithm::to_lower(s);
    return s;
}

class CurrencyException : public std::runtime_error
{
public:
    explicit CurrencyException(const std::string& msg)
      : std::runtime_error(msg)
    { }
};

Object DoRPCCall(const string& host,
                 uint32_t port,
                 const string& authentication,
                 const string& method, 
                 const Array& params)
{
    // Connect to host
    asio::io_service io_service;
    ssl::context context(io_service, ssl::context::sslv23);
    context.set_options(ssl::context::no_sslv2);
    asio::ssl::stream<asio::ip::tcp::socket> sslStream(io_service, context);
    SSLIOStreamDevice<asio::ip::tcp> d(sslStream, false);
    iostreams::stream< SSLIOStreamDevice<asio::ip::tcp> > stream(d);

    log_ << "rpc host is " << host << ":" << port << "\n";

    bool fConnected = d.connect(host, itostr(port));
    log_ << fConnected << "\n";
    if (!fConnected)
    {
        log_ << "couldn't connect!\n";
        throw CurrencyException("couldn't connect to server");
    }
    else
    {
        log_ << "connected\n";
    }

    // HTTP basic authentication
    StringMap headers;
    headers["Authorization"] = string("Basic ") + authentication;

    // Send request
    string request = JSONRPCRequest(method, params, 1);
    string post = HTTPPost(request, headers);
    stream << post << std::flush;

    log_ << "made request\n";
    // Receive HTTP reply status
    int protocol = 0;
    int status = ReadHTTPStatus(stream, protocol);

    // Receive HTTP reply message headers and body
    StringMap response_headers;
    string reply;
    ReadHTTPMessage(stream, response_headers, reply, protocol);

    if (status == HTTP_UNAUTHORIZED)
        throw CurrencyException(
            "incorrect rpcuser or rpcpassword (authorization failed)");
    else if (status >= 400 && status != HTTP_BAD_REQUEST 
                           && status != HTTP_NOT_FOUND 
                           && status != HTTP_INTERNAL_SERVER_ERROR)
        throw CurrencyException(
            strprintf("server returned HTTP error %d", status));
    else if (reply.empty())
        throw CurrencyException("no response from server");

    // Parse reply
    Value valReply;
    if (!read_string(reply, valReply))
        throw CurrencyException("couldn't parse reply from server");
    const Object& result = valReply.get_obj();
    if (result.empty())
        throw CurrencyException(
            "expected reply to have result, error and id properties");
    
    return result;
}

bool split_string(string whole, string& part1, string& part2, 
                  const char *delimiter=":")
{
    size_t delimiter_location = whole.find(delimiter);
    if (delimiter_location == string::npos)
        return false;
    part1 = whole.substr(0, delimiter_location);
    part2 = whole.substr(delimiter_location + 1,
                         whole.size() - delimiter_location - 1);
    return true;
}

bool GetMethodAndParamsFromCommand(string& method,
                                   vector<string>& params,
                                   string command)
{
    string part, remaining_part;
    if (command.find(" ") == string::npos)
    {
        method = command;
        return true;
    }
    if (!split_string(command, method, remaining_part, " "))
        return false;

    while (split_string(remaining_part, part, remaining_part, " "))
    {
        params.push_back(part);
    }
    params.push_back(remaining_part);
    return true;
}

template<typename T>
void ConvertTo(Value& value, bool fAllowNull=false)
{
    if (fAllowNull && value.type() == null_type)
        return;
    if (value.type() == str_type)
    {
        // reinterpret string as unquoted json value
        Value value2;
        string strJSON = value.get_str();
        if (!read_string(strJSON, value2))
            throw runtime_error(string("Error parsing JSON:")+strJSON);
        ConvertTo<T>(value2, fAllowNull);
        value = value2;
    }
    else
    {
        value = value.get_value<T>();
    }
}

Array ConvertParameterTypes(const string &method, 
                            const vector<string> &params)
{
    Array converted_params;
    foreach_(string param, params)
        converted_params.push_back(param);

    int n = converted_params.size();

    if (method == "sendtoaddress" && n > 1)
    {
        if (params[1].find(".") == string::npos)
            ConvertTo<int64_t>(converted_params[1]);
        else
            ConvertTo<double>(converted_params[1]);
    }
    if (method == "getreceivedbyaddress" && n > 1) 
        ConvertTo<int64_t>(converted_params[1]);
    if (method == "getbalance" && n > 1) 
        ConvertTo<int64_t>(converted_params[1]);
    if (method == "listunspent" && n > 0) 
        ConvertTo<int64_t>(converted_params[0]);
    if (method == "listunspent" && n > 1)
        ConvertTo<int64_t>(converted_params[1]);
    if (method == "listunspent"&& n > 2) 
        ConvertTo<Array>(converted_params[2]);
    if (method == "importprivkey" && n > 2)
        ConvertTo<bool>(converted_params[2]);

    return converted_params;
}

map<string,string> GetCurrencySettings(vch_t currency_code)
{
    map<string,string> settings;
    string currency_string(currency_code.begin(), currency_code.end());
    map<string,string>::const_iterator itr;
    for(itr = mapArgs.begin(); itr != mapArgs.end(); ++itr)
    {
        string key = itr->first;
        if (key.substr(1, currency_string.size() + 1) == currency_string + "-")
        {
            uint32_t start_subkey = 2 + currency_string.size();
            string subkey = key.substr(start_subkey, key.size());
            settings[subkey] = itr->second;
        }
    }
    return settings;
}

string CurrencyInfo(vch_t currency_code, string which_setting)
{
    string key = string("-") 
                 + string(currency_code.begin(), currency_code.end()) 
                 + string("-") + which_setting;
    
    if (mapArgs.count(key) == 0)
        return string("");
    
    return mapArgs[key];
}

string Replace(string text, string target, string replacement)
{
    size_t location;
    location = text.find(target);
    while (location != string::npos)
    {
        text = text.replace(location, target.size(), replacement);
        location = text.find(target);
    }
    return text;
}

bool Contains(string text, string target)
{
    size_t location = text.find(target);
    return location != string::npos;
}


string PubKeyToCurrencyAddress(Point pubkey, vch_t currency_code)
{
    Currency currency = flexnode.currencies[currency_code];

    if (currency.crypto.curve == SECP256K1)
    {
        vch_t version = currency.crypto.pubkey_prefix;

        CPubKey key(pubkey);
        if (!currency.crypto.compressed && key.IsCompressed())
            key.Decompress();
        CKeyID keyid = key.GetID();
        CBitcoinAddress address;
        address.SetData(version, &keyid, 20);
        return address.ToString();
    }
    string pubkey_to_address_command = CurrencyInfo(currency_code, 
                                                    "pubkeytoaddress");
    string command = Replace(pubkey_to_address_command, 
                             "PUBKEY", pubkey.ToString());

    string address = currency.rpc.ExecuteCommand(command).get_str();
    return Replace(address, "\n", "");
}

string PrivKeyToCurrencySecret(CBigNum privkey, vch_t currency_code)
{
    Currency currency = flexnode.currencies[currency_code];
    vch_t bytes = currency.crypto.privkey_prefix;

    log_ << "PrivKeyToCurrencySecret: prefix is " << bytes[0] << "\n";
    vch_t privkey_bytes = privkey.getvch32();

    reverse(privkey_bytes.begin(), privkey_bytes.end());
    
    log_ << "privkey bytes are: " << privkey_bytes << "\n";
    
    bytes.insert(bytes.end(), privkey_bytes.begin(), privkey_bytes.end());
    
    if (currency.crypto.compressed)
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

string GetStringBetweenQuotes(string text)
{
    if (Contains(text, "\""))
    {
        string preceding_characters, target, succeeding_characters;
        split_string(text, preceding_characters, target, "\"");
        split_string(target, text, succeeding_characters, "\"");
    }
    return text;
}

CBigNum CurrencySecretToPrivKey(string currency_secret, vch_t currency_code)
{
    currency_secret = GetStringBetweenQuotes(currency_secret);
    
    vch_t secret_bytes;
    CBigNum privkey;
    if (!DecodeBase58Check(currency_secret, secret_bytes))
    {
        log_ << "Failed to decode secret\n";
    }
    else
    {
        if (secret_bytes.size() == 34)
            secret_bytes.resize(33);
        reverse(secret_bytes.begin() + 1, secret_bytes.end());
       
        uint256 secret_256(vch_t(secret_bytes.begin() + 1,
                                 secret_bytes.end()));

        privkey = CBigNum(secret_256);
    }
    return privkey;
}

string CurrencyAddressBalanceCommand(vch_t currency, string address)
{
    string command = CurrencyInfo(currency, "getaddressbalance");
    command = Replace(command, "ADDRESS", address);
    return command;
}

string ImportPrivKeyCommand(vch_t currency, CBigNum privkey)
{
    string command = CurrencyInfo(currency, "importprivkey");
    string secret = PrivKeyToCurrencySecret(privkey, currency);
    command = Replace(command, "PRIVKEY", secret);
    return command;
}

string AmountToString(uint64_t amount, uint32_t decimal_places)
{
    char char_amount[50];
    if (decimal_places == 0)
    {
        sprintf(char_amount, "%lu", amount);
        return string(char_amount);
    }
    
    string string_amount = AmountToString(amount, 0);
    while (string_amount.size() < decimal_places)
        string_amount = "0" + string_amount;
    
    uint32_t l = string_amount.size();
    string_amount = string_amount.substr(0, l - decimal_places)
                    + "." +
                    string_amount.substr(l - decimal_places, decimal_places);

    if (string_amount.substr(0, 1) == ".")
        string_amount = "0" + string_amount;

    return string_amount;
}

uint32_t DecimalPlaces(vch_t currency)
{
    string decimal_places_string = CurrencyInfo(currency, "decimalplaces");
    if (decimal_places_string == "")
        return 8;
    return string_to_uint(decimal_places_string);
}

int64_t RealToAmount(double amount_float, vch_t currency)
{
    uint32_t decimal_places = DecimalPlaces(currency);
    uint64_t conversion_factor = 1;

    for (uint32_t i = 0; i < decimal_places; i++)
        conversion_factor *= 10;

    return roundint64(amount_float * conversion_factor);
}

string AmountToString(uint64_t amount, vch_t currency)
{
    return AmountToString(amount, DecimalPlaces(currency));
}

int64_t ExtractBalanceFromString(string input_string, vch_t currency)
{
    uint32_t decimal_places = DecimalPlaces(currency);

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
    
    while (balance_string.length() 
            <= balance_string.find(".") + decimal_places)
        balance_string = balance_string + "0";
    log_ << "balance_string is now " << balance_string << "\n";
    balance_string = balance_string.replace(balance_string.find("."), 1, "");
    log_ << "balance_string is now " << balance_string << "\n";
    while (balance_string.substr(0, 1) == "0")
        balance_string = balance_string.replace(balance_string.find("0"), 1, "");
    log_ << "returning " << string_to_uint(balance_string) << "\n";
    return string_to_uint(balance_string);
}

bool txid_and_n_from_payment_proof(vch_t payment_proof,
                                   string& txid,
                                   uint32_t& n)
{
    string proof(payment_proof.begin(), payment_proof.end());
    string string_n;
    if (!split_string(proof, txid, string_n))
        return false;
    n = string_to_uint(string_n);
    return true;
}

vch_t payment_proof_from_txid_and_n(string txid, uint32_t n)
{
    string proof = txid + ":" + AmountToString(n, 0);
    return vch_t(proof.begin(), proof.end());
}

vector<Unspent> ExtractUnspentFromOutput(string json_output, vch_t currency)
{
    vector<Unspent> unspent;
    Value list_of_outputs_value;
    Array list_of_outputs;

    log_ << "extracting unspent\n";
    if (!json_spirit::read(json_output, list_of_outputs_value))
    {
        log_ << "ExtractUnspentFromOutput: Can't parse: " 
             << json_output << "\n";
        return unspent;
    }
    
    list_of_outputs = list_of_outputs_value.get_array();
    for (uint32_t i = 0; i < list_of_outputs.size(); i++)
    {
        Object output = list_of_outputs[i].get_obj();
        if (Contains(json_output, "scriptPubKey") &&
            Contains(json_output, "type"))
        {
            Object script_pubkey = find_value(output, "scriptPubKey").get_obj();
            if (find_value(script_pubkey, "type").get_str() != "pubkeyhash")
                continue;
        }
        
        Unspent item;
        if (Contains(json_output, "txid"))
            item.txid = find_value(output, "txid").get_str();
        else if (Contains(json_output, "prevout_hash"))
            item.txid = find_value(output, "prevout_hash").get_str();
        else
            continue;
        
        if (Contains(json_output, "prevout_n"))
        {
            int n;
            n = find_value(output, "prevout_n").get_int();
            log_ << "n = " << n << "\n";
            item.n = n;
        }
        else if (Contains(json_output, "vout"))
            item.n = find_value(output, "vout").get_int();

        if (Contains(json_output, "amount"))
        {
            item.amount = RealToAmount(find_value(output, 
                                                  "amount").get_real(),
                                       currency);
        }
        else if (Contains(json_output, "value"))
        {
            try
            {
                item.amount = RealToAmount(
                                    find_value(output, "value").get_real(),
                                    currency);
            }
            catch(...)
            {
                string value_string = find_value(output, "value").get_str();
                value_string = Replace(value_string, "\"", "");
                item.amount = ExtractBalanceFromString(value_string, currency);
            }
        }
        else
            continue;

        if (Contains(json_output, "confirmations"))
            item.confirmations = find_value(output, "confirmations").get_int();
        else
            item.confirmations = 1;

        item.address = find_value(output, "address").get_str();
        unspent.push_back(item);
    }
    return unspent;
}


CurrencyTxOut CurrencyTxOutFromJSON(string json, vch_t currency)
{
    CurrencyTxOut txout;
    Value json_value;
    Object json_object;
    if (!json_spirit::read(json, json_value))
    {
        cerr << "CurrencyTxOutFromJSON: Can't parse: "  << json << "\n";
        return txout;
    }
    json_object = json_value.get_obj();
    Object script_pubkey = find_value(json_object, "scriptPubKey").get_obj();
    if (find_value(script_pubkey, "type").get_str() != "pubkeyhash")
        return txout;
    Array addresses = find_value(script_pubkey, "addresses").get_array();
    if (addresses.size() != 1)
        return txout;

    txout.amount = RealToAmount(find_value(json_object, "value").get_real(),
                                currency);
    txout.address = addresses[0].get_str();
    txout.confirmations = find_value(json_object, "confirmations").get_int();
    return txout;
}


vch_t SecretBytesFromDumpedPrivKey(string dumped_privkey)
{
    CBitcoinSecret secret;
    if (!secret.SetString(dumped_privkey))
        log_ << "Failed to set secret to string: " << dumped_privkey << "\n";
#ifndef _PRODUCTION_BUILD
    else
        log_ << "Succeeded in setting secret to string: "
             << dumped_privkey << "\n";
#endif
    vch_t secret_bytes(&secret.vchData[0], 
                       &secret.vchData[0] + secret.vchData.size());
   
    return secret_bytes;
}

vch_t PrefixFromBase58Data(string base58string)
{
    CBase58Data data;
    data.SetString(base58string);
    return data.vchVersion;
}



/*********************
 *  CurrencyCommands
 */

    CurrencyCommands::CurrencyCommands(vch_t currency_code):
                                       currency_code(currency_code)    
    {
        StringMap command_strings = GetCurrencySettings(currency_code);

        foreach_(StringMap::value_type item, command_strings)
        {
            commanddata[currency_code][item.first] = item.second;
        }
    }

    string CurrencyCommands::BalanceCommand()
    {
        return commanddata[currency_code]["getbalance"];
    }

    string CurrencyCommands::AddressBalanceCommand(string address)
    {
        string command = commanddata[currency_code]["getaddressbalance"];
        return Replace(command, "ADDRESS", address);
    }

    string CurrencyCommands::GetAddressWithAmountAndProof(uint64_t amount)
    {
        string command = commanddata[currency_code]["getaddresswithamountandproof"];
        return Replace(command, "AMOUNT", AmountToString(amount));
    }

    string CurrencyCommands::SendToAddressCommand(string address,
                                                  uint64_t amount)
    {
        string command;
        if (commanddata[currency_code].HasProperty("sendtoaddress"))
        {
            string command_ = commanddata[currency_code]["sendtoaddress"];
            command = command_;
        }
        else if (commanddata[currency_code].HasProperty("payto"))
        {
            string command_ = commanddata[currency_code]["payto"];
            command = command_;
        }

        command = Replace(command, "ADDRESS", address);
        command = Replace(command, "AMOUNT", AmountToString(amount));
        return command;
    }

    string CurrencyCommands::SendFiatCommand(vch_t my_data, 
                                             vch_t their_data,
                                             uint64_t amount)
    {
        string command = commanddata[currency_code]["sendfiat"];
        string my_data_string(my_data.begin(), my_data.end());
        string their_data_string(their_data.begin(), their_data.end());
        command = Replace(command, "AMOUNT", AmountToString(amount));
        command = Replace(command, "MY_DATA", my_data_string);
        command = Replace(command, "THEIR_DATA", their_data_string);
        return command;
    }

    string CurrencyCommands::ValidateProposedFiatTransactionCommand(
                                             uint160 accept_commit_hash,
                                             vch_t payer_data,
                                             vch_t payee_data,
                                             uint64_t amount)
    {
        string command = commanddata[currency_code]["notaryvalidate"];
        string payer_data_string(payer_data.begin(), payer_data.end());
        string payee_data_string(payee_data.begin(), payee_data.end());
        command = Replace(command, "ACCEPT_COMMIT_HASH", 
                          accept_commit_hash.ToString());
        command = Replace(command, "AMOUNT", AmountToString(amount));
        command = Replace(command, "PAYER_DATA", payer_data_string);
        command = Replace(command, "PAYEE_DATA", payee_data_string);
        return command;
    }

    string CurrencyCommands::VerifyFiatPaymentCommand(vch_t payment_proof,
                                                      vch_t payer_data, 
                                                      vch_t payee_data,
                                                      uint64_t amount)
    {
        string command = commanddata[currency_code]["notaryverify"];
        command = Replace(command, "PAYMENT_PROOF",
                          string(payment_proof.begin(), payment_proof.end()));
        command = Replace(command, "PAYER_DATA",
                          string(payer_data.begin(), payer_data.end()));
        command = Replace(command, "PAYEE_DATA",
                          string(payee_data.begin(), payee_data.end()));
        command = Replace(command, "AMOUNT", AmountToString(amount));
        return command;
    }

    string CurrencyCommands::AmountToString(uint64_t amount)
    {
        return AmountToString(amount, DecimalPlaces(currency_code));
    }

    string CurrencyCommands::AmountToString(uint64_t amount, 
                                            uint32_t decimal_places)
    {
        char char_amount[50];
        if (decimal_places == 0)
        {
            sprintf(char_amount, "%lu", amount);
            return string(char_amount);
        }
        
        string string_amount = AmountToString(amount, 0);
        while (string_amount.size() < decimal_places)
            string_amount = "0" + string_amount;
        
        uint32_t l = string_amount.size();
        string_amount = string_amount.substr(0, l - decimal_places)
                        + "." +
                        string_amount.substr(l - decimal_places, decimal_places);

        if (string_amount.substr(0, 1) == ".")
            string_amount = "0" + string_amount;

        return string_amount;
    }
/*
 *  CurrencyCommands
 *********************/


/*************
 *  Currency
 */

    Currency::Currency(vch_t currency_code, bool is_cryptocurrency):
        currency_code(currency_code),
        is_cryptocurrency(is_cryptocurrency),
        crypto(currency_code)
    {
        commands = CurrencyCommands(currency_code);
        rpc = CurrencyRPC(currency_code);

    }

    Currency::Currency(const Currency& other):
        currency_code(other.currency_code),
        is_cryptocurrency(other.is_cryptocurrency),
        crypto(other.crypto),
        rpc(other.rpc)
    {
        commands = CurrencyCommands(currency_code);
    }

    int64_t Currency::Balance()
    {
        Value result;
        try
        {
            result = rpc.ExecuteCommand(commands.BalanceCommand());
        }
        catch (CurrencyException& e) { return -1; }
        log_ << "Balance(): result is " << result.get_str() << "\n";
        return ExtractBalanceFromString(result.get_str(), currency_code);
    }

    int64_t Currency::GetAddressBalance(string address)
    {
        string balance_command  = commands.AddressBalanceCommand(address);
        Value result;
        try
        {
            result = rpc.ExecuteCommand(balance_command);
        } 
        catch (CurrencyException& e) { return -1; }

        return ExtractBalanceFromString(result.get_str(), currency_code);
    }

    string Currency::SendToAddress(string currency_address,
                                   uint64_t amount)
    {
        string command = commands.SendToAddressCommand(currency_address,
                                                       amount);
        if (command == string(""))
            return string("");
        string result = rpc.ExecuteCommand(command).get_str();
        log_ << "SendToAddress: result " << result << "\n";
        if (is_cryptocurrency)
        {
            if (crypto.Supports("payto") && !crypto.Supports("sendtoaddress"))
            {
                if (!Contains(result, "\"hex\":"))
                {
                    log_ << "Tried to use payto, but got no hex\n";
                    return result;
                }
                string before, after, hex;
                split_string(result, before, after, "\"hex\":");
                hex = GetStringBetweenQuotes(after);
                log_ << "broadcasting hex\n";
                return crypto.Broadcast(hex);
            }
        }
        return result;
    }

    bool Currency::GetAddressAndProofOfFunds(string& address, 
                                             string& proof, 
                                             uint64_t amount)
    {
        if (is_cryptocurrency)
        {
            CurrencyTxOut txout = crypto.GetTxOutWithAmount(amount);
            if (!txout)
                return false;
            address = txout.address;
            proof = txout.txid + ":" + AmountToString(txout.n, 0);
            return true;
        }
        string command = commands.GetAddressWithAmountAndProof(amount);
        if (command == "")
            return true;
        string output = rpc.ExecuteCommand(command).get_str();
        split_string(output, address, proof);
        return true;
    }

    string Currency::SendFiat(vch_t my_data, 
                              vch_t their_data, 
                              uint64_t amount)
    {
        string command = commands.SendFiatCommand(my_data, their_data, amount);
        if (command == string(""))
        {
            string payer(my_data.begin(), my_data.end());
            string payee(their_data.begin(), their_data.end());
            string url = CurrencyInfo(currency_code, "payment_url");
            if (url == "")
                return "";
            uint160 prompt_hash = GetTimeMicros();
            guidata[prompt_hash]["payment_url"] = url;
            guidata[prompt_hash]["payer"] = payer;
            guidata[prompt_hash]["payee"] = payee;
            guidata[prompt_hash].Location("events") = GetTimeMicros();
            string text_prompt("Please send " 
                                + AmountToString(amount, currency_code)
                                + " to " + payee + " from " + payer
                                + " by going to " + url + "\n");
            printf("%s\n", text_prompt.c_str());
            fprintf(stderr, "%s\n", text_prompt.c_str());
            log_ << text_prompt;
            return "";
        }
        return rpc.ExecuteCommand(command).get_str();
    }

    bool Currency::ValidateProposedFiatTransaction(uint160 accept_commit_hash,
                                                   vch_t payer_data, 
                                                   vch_t payee_data, 
                                                   uint64_t amount)
    {
        string command = commands.ValidateProposedFiatTransactionCommand(
                                                    accept_commit_hash,
                                                    payer_data, 
                                                    payee_data, 
                                                    amount);
        if (command == string(""))
            return false;
        NotaryRPC notaryrpc(currency_code);
        string result = notaryrpc.ExecuteCommand(command).get_str();
        log_ << "VerifyProposedFiatTransaction: result is " << result << "\n";
        return Replace(result, "\n", "") == "ok";
    }

    bool Currency::VerifyFiatPayment(vch_t payment_proof,
                                     vch_t payer_data,
                                     vch_t payee_data,
                                     uint64_t amount)
    {
        if (is_cryptocurrency)
            return false;
        string command = commands.VerifyFiatPaymentCommand(payment_proof, 
                                                           payer_data,
                                                           payee_data, 
                                                           amount);
        if (command == string(""))
            return false;
        NotaryRPC notaryrpc(currency_code);
        string result = notaryrpc.ExecuteCommand(command).get_str();
        log_ << "VerifyFiatPayment: result is " << result << "\n";
        return Replace(result, "\n", "") == "ok";
    }

    int64_t Currency::RealToAmount(double amount_float, vch_t currency)
    {
        uint32_t decimal_places = DecimalPlaces(currency);
        uint64_t conversion_factor = 1;

        for (uint32_t i = 0; i < decimal_places; i++)
            conversion_factor *= 10;

        return roundint64(amount_float * conversion_factor);
    }
    
    int64_t Currency::RealToAmount(double amount_float)
    {
        return RealToAmount(amount_float, currency_code);
    }

/*
 *  Currency
 *************/


/*******************
 *  CurrencyCrypto
 */

    CurrencyCrypto::CurrencyCrypto(vch_t currency_code):
        currency_code(currency_code),
        rpc(currency_code),
        compressed(false)
    {
        commands = CryptoCurrencyCommands(currency_code);
    }

    void CurrencyCrypto::Initialize()
    {
        DetermineCurve();
        DetermineCapabilities();
        if (!usable)
            return;
        commands.SetCommands();
        DetermineCompressed();
        
        DeterminePrefixes();
        
        locked = IsLocked();
    }

    void CurrencyCrypto::DetermineCurve()
    {
        string curve_name = lowercase(CurrencyInfo(currency_code, "curve"));
        if (curve_name == "ed25519")
            curve = ED25519;
        else if (curve_name == "curve25519")
            curve = CURVE25519;
        else
            curve = SECP256K1;
    }

    void CurrencyCrypto::DetermineCompressed()
    {
        string dumped_privkey = GetStringBetweenQuotes(GetAnyDumpedPrivKey());
        vch_t secret_bytes;
        if (!DecodeBase58Check(dumped_privkey, secret_bytes))
        {
            log_ << "DetermineCompressed(): Failed to decode secret\n";
            return;
        }
        else
        {
            log_ << "size of secret bytes is " << secret_bytes.size() << "\n";
            if (secret_bytes.size() == 34)
                compressed = true;
        }
        log_ << "compressed is " << compressed << "\n";
    }

    void CurrencyCrypto::DeterminePrefixes()
    {
        vch_t prefix = currencydata[currency_code]["pubkey_prefix"];
        pubkey_prefix = prefix;
        vch_t prefix_ = currencydata[currency_code]["privkey_prefix"];
        privkey_prefix = prefix_;

        string address = GetAnyAddress();
        log_ << "got address: " << address << "\n";
        if (privkey_prefix.size() == 0)
        {
            if (address.size() == 0)
            {
                usable = false;
                return;
            }
            pubkey_prefix = PrefixFromBase58Data(address);
            string dumped_privkey = GetAnyDumpedPrivKey();
            dumped_privkey = GetStringBetweenQuotes(dumped_privkey);

            if (IsLocked())
            {
                log_ << "locked - not getting privkey prefix\n";
                return;
            }
            
            if (dumped_privkey.size() == 0)
            {
                usable = false;
                return;
            }
            privkey_prefix = PrefixFromBase58Data(dumped_privkey);
            if (privkey_prefix.size() == 0)
            {
                usable = false;
                return;
            }
            log_ << "pubkey prefix is " << pubkey_prefix << "\n";
#ifndef _PRODUCTION_BUILD
            log_ << "got prefix " << privkey_prefix[0] << " from "
                 << dumped_privkey << "\n";
#endif
        }
    }

    void CurrencyCrypto::DetermineCapabilities()
    {
        usable = true;
        vch_t cc = currency_code;
        string strcc(cc.begin(), cc.end());
        if (!currencydata[cc]["determined"])
        {
            string_t help_command("help");
            string help;
            try
            {
                Value response = rpc.ExecuteCommand(help_command);
                help = rpc.ExecuteCommand(help_command).get_str();
            }
            catch (CurrencyException& e)
            {
                log_ << "Failed to get help from " << strcc << "\n"
                     << "Currency is unusable\n";
                usable = false;
                return;
            }
            catch(...)
            {
                log_ << "Encountered exception\n";
            }
            log_ << "output from " << help_command << " is " << help << "\n";

            if (help == "")
            {
                log_ << "No help for " << strcc <<  " - unusable.\n";
                usable = false;
                return;
            }

            string help_nospace = Replace(help, " ", "");
            log_ << "HELP NOSPACE IS " << help_nospace << "\n";
            foreach_(const StringMap::value_type& item, default_commands)
            {
                string command_name = item.first;
                string command_string = item.second;
                currencydata[cc][command_name]
                    = Contains(help, command_name) &&
                      ! Contains(help_nospace, command_name + "Deprecated") &&
                      ! Contains(help_nospace, command_name + "sDeprecated");
                log_ << "supports[" << command_name << "]="
                     << (bool)currencydata[cc][command_name] << "\n";
            }
        }
        else
        {
        }
    }

    bool CurrencyCrypto::Supports(string_t command_name)
    {
        return currencydata[currency_code][command_name];
    }

    void CurrencyCrypto::DetermineUsable()
    {
        if (!usable)
            return;
        vch_t cc = currency_code;
        string strcc(cc.begin(), cc.end());
        if (!(Supports("sendtoaddress") || Supports("payto")))
        {
            log_ << "No way to pay! Must support sendtoaddress or payto!\n";
            usable = false;
        }
        if (!(Supports("importprivkey") || Supports("sweep") 
                                        || Supports("sweepprivkey")))
        {
            log_ << "No way to import private keys!";
            log_ << "Must support importprivkey, sweep or sweepprivkey!\n";
            usable = false;
        }
        if (!(Supports("dumpprivkey") || Supports("getprivatekeys")))
        {
            log_ << "No way to export private keys!";
            log_ << "Must support dumpprivkey or getprivatekeys!\n";
            usable = false;
        }
        if (curve != SECP256K1 && 
            CurrencyInfo(currency_code, "pubkey_to_address_command") == "")
        {
            log_ << "Non-Secp256k1 currencies must specify ";
            log_ << "pubkey_to_address_command in config!\n";
            usable = false;
        }
        if (!(Supports("gettxout") || 
              Supports("import_address") || Supports("getaddressbalance")))
        {
            log_ << "No way to check payments have been made!\n"
                 << "Currency must support one of:\n"
                 << "gettxout\n"
                 << "importaddress\n"
                 << "getaddressbalance\n"
                 << "or define " << strcc 
                 << "-getaddressbalance in the config file\n";
                   
            usable = false;
        }
        if (!usable)
            log_ << "currency " << strcc << " is unusable\n";
    }

    string CurrencyCrypto::Broadcast(string data)
    {
        if (!Supports("broadcast"))
            return "";
        string command = commands.BroadcastCommand(data);
        return rpc.ExecuteCommand(command).get_str();
    }

    string CurrencyCrypto::AddNToTxId(string txid, 
                                      Point pubkey, 
                                      uint64_t amount)
    {
        uint32_t n = 0;
        string address = PubKeyToAddress(pubkey);
        log_ << "looking for txout with txid " << txid 
             << " and amount " << amount << "\n";
        while (true)
        {
            CurrencyTxOut txout = GetTxOut(txid, n);
            if (!txout)
                break;
            log_ << "txout has amount " << txout.amount << "\n";
            if (txout.address == address && txout.amount == amount)
            {
                char str_n[5];
                sprintf(str_n, ":%u", n);
                log_ << "n is " << n << "\n";
                return txid + string(str_n);
            }
            n++;
        }
        log_ << "Couldn't find txout\n";
        return "";
    }

    string CurrencyCrypto::SendToPubKey(Point pubkey, uint64_t amount)
    {
        string command = commands.SendToPubKeyCommand(pubkey, amount);
        log_ << "executing command: " << command << "\n";
        string output = rpc.ExecuteCommand(command).get_str();
        output.erase(remove(output.begin(), output.end(), '\n'), output.end());
        return AddNToTxId(output, pubkey, amount);
    }

    string CurrencyCrypto::PubKeyToAddress(Point pubkey)
    {
        return PubKeyToCurrencyAddress(pubkey, currency_code);
    }

    string CurrencyCrypto::PrivKeyToSecret(CBigNum privkey)
    {
        return PrivKeyToCurrencySecret(privkey, currency_code);
    }

    int64_t CurrencyCrypto::GetPubKeyBalance(Point pubkey)
    {
        string command = commands.GetPubKeyBalanceCommand(pubkey);
        string result = rpc.ExecuteCommand(command).get_str();
        return ExtractBalanceFromString(result, currency_code);
    }

    bool CurrencyCrypto::ImportPrivateKey(CBigNum privkey)
    {
        string command = commands.ImportPrivateKeyCommand(privkey);
        if (command == string(""))
            return false;
#ifndef _PRODUCTION_BUILD
        log_ << "private key is: " << privkey << "\n";
#endif
        Point pubkey(curve, privkey);

        log_ << "public key is: " << pubkey << "\n";
        string address = PubKeyToAddress(pubkey);
        log_ << "address is " << address << "\n";
#ifndef _PRODUCTION_BUILD
        log_ << "executing: " << command << "\n";
#endif
        string result = rpc.ExecuteCommand(command).get_str();
        if (result.find(":") != string::npos)
        {
            if (Contains(result, "imported"))
                return true;
            cerr << "ImportCryptoCurrencyPrivateKey: failed: "
                 << result << "\n";
            return false;
        }
        return true;
    }

    bool CurrencyCrypto::ImportPrivateKey(string privkey_string)
    {
        CBigNum privkey = CurrencySecretToPrivKey(privkey_string,
                                                  currency_code);
        return ImportPrivateKey(privkey);
    }

    string CurrencyCrypto::DumpPrivKey(string address)
    {
        string command = commands.GetPrivateKeyCommand(address);
        string privkey = rpc.ExecuteCommand(command).get_str();
#ifndef _PRODUCTION_BUILD
        log_ << "dump privkey command = " << command << "\n";
        log_ << "privkey = " << privkey << "\n"
             << "number = "
             << CurrencySecretToPrivKey(privkey, currency_code) << "\n";
#endif
        return privkey;
    }

    string CurrencyCrypto::GetAnyDumpedPrivKey()
    {
        string address = GetAnyAddress();
        return DumpPrivKey(address);
    }

    string CurrencyCrypto::GetAnyAddress()
    {
        log_ << "trying to get unspent\n";
        vector<Unspent> unspent_outputs = ListUnspent();
        log_ << "got " << unspent_outputs.size() << " unspent\n";
        if (unspent_outputs.size() > 0)
            return unspent_outputs[0].address;
        
        if (Supports("getnewaddress"))
            return GetNewAddress();

        if (Supports("listaddresses"))
        {
            vector<string> addresses = ListAddresses();
            if (addresses.size() > 0)
                return addresses[0];
        }
        string cc(currency_code.begin(), currency_code.end());
        log_ << "Failed to get any address for " << cc << "\n";
        return "";
    }

    string CurrencyCrypto::GetNewAddress()
    {
        string command = commands.GetNewAddressCommand();
        log_ <<"new address command: " << command << "\n";
        return rpc.ExecuteCommand(command).get_str();
    }

    vector<string> CurrencyCrypto::ListAddresses()
    {
        string command = commands.ListAddressesCommand();
        log_ << "list addresses command: " << command << "\n";
        string list_address_output = rpc.ExecuteCommand(command).get_str();
        vector<string> addresses;
        string address, rest;

        while (Contains(list_address_output, ","))
        {
            split_string(list_address_output, address, rest, ",");
            address = GetStringBetweenQuotes(address);
            if (address.size() > 0 and !Contains(address, " "))
                addresses.push_back(address);
            list_address_output = rest;
        }
        address = GetStringBetweenQuotes(list_address_output);
        if (address.size() > 0 and !Contains(address, " "))
            addresses.push_back(address);
        log_ << "addresses are " << addresses << "\n";
        return addresses;


    }

    string CurrencyCrypto::MakeTradeable(uint64_t amount)
    {
        Point pubkey(curve, GetPrivateKey(GetNewAddress()));
        return SendToPubKey(pubkey, amount);
    }

    CBigNum CurrencyCrypto::GetPrivateKey(string address)
    {
        string dumped_privkey = DumpPrivKey(address);
        CBigNum privkey = CurrencySecretToPrivKey(dumped_privkey, 
                                                  currency_code);
        log_ << "privkey for " << dumped_privkey << " is "
             << privkey << "\n";
        log_ << "pubkey is " << Point(curve, privkey) << "\n";
        log_ << "keyhash is " << KeyHash(Point(curve, privkey)) << "\n";
        log_ << "address is " << PubKeyToAddress(Point(curve, privkey)) << "\n";
        keydata[Point(curve, privkey)]["privkey"] = privkey;
        return privkey;
    }

    vector<Unspent> CurrencyCrypto::ListUnspent()
    {
        string command = commands.ListUnspentCommand();
        log_ << "listunspentcommand: " << command << "\n";
        string json_output = rpc.ExecuteCommand(command).get_str();
        log_ << "output: " <<  json_output << "\n";
        return ExtractUnspentFromOutput(json_output, currency_code);
    }

    Unspent CurrencyCrypto::GetUnspentWithAmount(uint64_t amount)
    {
        vector<Unspent> unspent_outputs = ListUnspent();
        foreach_(const Unspent& unspent, unspent_outputs)
        {
            if (unspent.amount >= amount)
                return unspent;
        }
        Unspent empty_unspent;
        return empty_unspent;
    }

    CurrencyTxOut CurrencyCrypto::GetTxOut(string txid, uint32_t n)
    {
        string command = commands.GetTxOutCommand(txid, n);
        log_ << "GetTxOutCommand: " <<  command << "\n";
        string json_output = rpc.ExecuteCommand(command).get_str();
        log_ << "json = " << json_output << "\n";
        CurrencyTxOut txout = CurrencyTxOutFromJSON(json_output, 
                                                    currency_code);
        txout.txid = txid;
        txout.n = n;
        return txout;
    }

    CurrencyTxOut CurrencyCrypto::GetTxOutWithAmount(uint64_t amount)
    {
        log_ << "GetTxOutWithAmount: " << amount << "\n";
        Unspent unspent = GetUnspentWithAmount(amount);
        return GetTxOut(unspent.txid, unspent.n);
    }

    bool CurrencyCrypto::VerifyPayment(string payment_proof,
                                       Point pubkey, uint64_t amount, 
                                       uint32_t confirmations_required)
    {
        string txid;
        uint32_t n;
        log_ << "verifying payment with proof: " << payment_proof << "\n";
        vch_t proof(payment_proof.begin(), payment_proof.end());
        if (!txid_and_n_from_payment_proof(proof, txid, n))
        {
            log_ << "couldn't get txid and n\n";
            return false;
        }

        return VerifyPayment(txid, n, pubkey, amount, confirmations_required);
    }

    bool CurrencyCrypto::VerifyPayment(string txid, uint32_t n, 
                                       Point pubkey, uint64_t amount, 
                                       uint32_t confirmations_required)
    {
        log_ << "VerifyPayment(): order pubkey=" << pubkey << "\n";
        string address = PubKeyToAddress(pubkey);
        if (Supports("gettxout"))
        {
            log_ << "using gettxout: " << txid << n << "\n";
            CurrencyTxOut txout = GetTxOut(txid, n);
            if (txout.confirmations < confirmations_required)
            {
                log_ << "not enough confirmations: "
                     << txout.confirmations << " vs " 
                     << confirmations_required << "\n";
                return false;
            }
            log_ << "address check: " << txout.address << " vs " 
                 << address << "\n";
            return txout.address == address && txout.amount >= amount;

        }
        else if (Supports("import_address"))
        {
            log_ << "using getpubkeybalance: " << pubkey << "\n";
            int64_t balance = GetPubKeyBalance(pubkey);
            return balance > 0 && (uint64_t)balance >= amount;
        }
        log_ << "supports neither gettxout nor import_address\n";
        return false;
    }

    bool CurrencyCrypto::IsLocked()
    {
        string command = commands.InfoCommand();
        log_ << "commands.InfoCommand() is " << command << "\n";
        string info_json = rpc.ExecuteCommand(command).get_str();
        log_ << "IsLocked(): info_json is " << info_json << "\n";
        Object info_object;
        Value json_value;
        if (!json_spirit::read(info_json, json_value))
        {
            cerr << "IsLocked(): Can't parse info string: " 
                 << info_json << "\n";
            if (Contains(info_json, "lectrum"))
            {
                currencydata[currency_code]["electrum"] = true;
                return false;
            }
            return true;
        }
        info_object = json_value.get_obj();
        Value unlock_time_value = find_value(info_object, "unlocked_until");
        
        if (unlock_time_value.is_null())
            locked = false;
        else if (unlock_time_value.get_int() == 0)
            locked = true;
        else if (unlock_time_value.get_int() > GetTimeMicros() / 1e6)
            locked = false;
        return locked;
    }

/*
 *  CurrencyCrypto
 *******************/



/***************************
 *  CryptoCurrencyCommands
 */

    CryptoCurrencyCommands::CryptoCurrencyCommands(vch_t currency_code):
        currency_code(currency_code)
    { }

    void CryptoCurrencyCommands::SetCommands()
    {
        vch_t cc = currency_code;
        log_ << "CryptoCurrencyCommands::SetCommands\n";
        foreach_(const StringMap::value_type& item, default_commands)
        {
            string command_name = item.first;
            string command_string = item.second;
            log_ << command_name << ": " << command_string << "\n";
            if (currencydata[cc][command_name])
            {
                commanddata[cc][command_name] = command_string;
            }
            else if (CurrencyInfo(cc, command_name) != "")
            {
                commanddata[cc][command_name] = CurrencyInfo(cc,
                                                             command_name);
                currencydata[cc][command_name] = true;
            }
        }
    }

    string CryptoCurrencyCommands::ImportPrivateKeyCommand(CBigNum privkey)
    {
        string secret = PrivKeyToCurrencySecret(privkey, currency_code);
        Currency currency = flexnode.currencies[currency_code];
        string command = commanddata[currency_code]["importprivkey"];
        if (currencydata[currency_code]["electrum"])
        {
            command = Replace(command, " flex false", "");
        }
        return Replace(command, "PRIVKEY", secret);
    }

    string CryptoCurrencyCommands::GetPrivateKeyCommand(string address)
    {
        string command;
        log_ << "GetPrivateKeyCommand: " << address << "\n";

        if (currencydata[currency_code]["dumpprivkey"])
        {
            log_ << "supports dumpprivkey!\n";
            string command_ = commanddata[currency_code]["dumpprivkey"];
            command = command_;
            log_ << "command = " << command << "\n";
        }
        else if (currencydata[currency_code]["getprivatekeys"])
        {
            string command_ = commanddata[currency_code]["getprivatekeys"];
            command = command_;
        }
        else
        {
            log_ << "Neither dumpprivkey nor getprivatekeys are supported!\n";
        }
        return Replace(command, "ADDRESS", address);
    }

    string CryptoCurrencyCommands::SendToPubKeyCommand(Point pubkey, 
                                                       uint64_t amount)
    {
        string address = PubKeyToCurrencyAddress(pubkey, currency_code);
        Currency currency = flexnode.currencies[currency_code];
        return currency.commands.SendToAddressCommand(address, amount);
    }

    string CryptoCurrencyCommands::ImportPubKeyCommand(Point pubkey)
    {
        string address = PubKeyToCurrencyAddress(pubkey, currency_code); 
        string command = commanddata[currency_code]["importaddress"];
        return Replace(command, "ADDRESS", address);
    }

    string CryptoCurrencyCommands::GetPubKeyBalanceCommand(Point pubkey)
    {
        string address = PubKeyToCurrencyAddress(pubkey, currency_code);
        string command = commanddata[currency_code]["getaddressbalance"];
        return Replace(command, "ADDRESS", address);
    }

    string CryptoCurrencyCommands::ListUnspentCommand()
    {
        return commanddata[currency_code]["listunspent"];
    }

    string CryptoCurrencyCommands::BroadcastCommand(string data)
    {
        string command = commanddata[currency_code]["broadcast"];
        return Replace(command, "DATA", data);
    }

    string CryptoCurrencyCommands::GetNewAddressCommand()
    {
        return commanddata[currency_code]["getnewaddress"];
    }

    string CryptoCurrencyCommands::ListAddressesCommand()
    {
        return commanddata[currency_code]["listaddresses"];
    }

    string CryptoCurrencyCommands::HelpCommand()
    {
        return commanddata[currency_code]["help"];
    }

    string CryptoCurrencyCommands::InfoCommand()
    {
        return commanddata[currency_code]["getinfo"];
    }

    string CryptoCurrencyCommands::GetTxOutCommand(string txid, uint32_t n)
    {
        string position = AmountToString(n, 0);
        string command = commanddata[currency_code]["gettxout"];
        command = Replace(command, "TXID", txid);
        command = Replace(command, "N", position);
        return command;
    }

    string CryptoCurrencyCommands::UnlockWalletCommand(string password)
    {
        string unlock_time = AmountToString(WALLET_UNLOCK_TIME, 0);
        string command = commanddata[currency_code]["walletpassphrase"];
        command = Replace(command, "PASSPHRASE", password);
        command = Replace(command, "UNLOCK_TIME", unlock_time);
        return command;
    }

/*
 *  CryptoCurrencyCommands
 ***************************/

void InitializeCryptoCurrencies()
{
    map<string,string>::const_iterator itr;
    for(itr = mapArgs.begin(); itr != mapArgs.end(); ++itr)
    {
        string key = itr->first;
        string key_ = key.substr(1, key.size() - 1);
        if (Contains(key_, "-rpcport"))
        {
            string currency_string = Replace(key_, "-rpcport", "");
            vch_t currency_code(currency_string.begin(),
                                currency_string.end());
            log_ << "Found port for " << currency_string << "\n";
            bool is_cryptocurrency = CurrencyInfo(currency_code, "fiat") == "";
            Currency currency(currency_code, is_cryptocurrency);
            log_ << "Generated currency: " << currency_string << "\n";
            flexnode.currencies[currency_code] = currency;
            if (is_cryptocurrency)
            {
                currency.crypto.Initialize();
            }
            if (!is_cryptocurrency || currency.crypto.usable)
                flexnode.currencies[currency_code] = currency;
        }
    }
    log_ << "finished initializing currencies\n";
    std::map<vch_t,Currency>::iterator it;
    std::map<vch_t,Currency>& currencies = flexnode.currencies;
    for (it = currencies.begin(); it != currencies.end(); ++it)
        log_ << string(it->first.begin(), it->first.end()) << "\n";
}
