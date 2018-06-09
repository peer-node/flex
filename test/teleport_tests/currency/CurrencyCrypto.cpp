#include <src/base/base58.h>
#include <src/credits/creditsign.h>
#include <src/base/BitcoinAddress.h>
#include "CurrencyCrypto.h"
#include "Currency.h"


using std::string;
using std::vector;

#include "log.h"
#define LOG_CATEGORY "CurrencyCrypto.cpp"
//
//
//
//CurrencyCrypto::CurrencyCrypto(std::string currency_code, TeleportConfig &config):
//    config(config),
//    currency_code(currency_code),
//    rpc(currency_code, config),
//    commands(currency_code, config),
//    compressed(false)
//{
//    commands.SetCurrencyCrypto(this);
//}
//
//string CurrencyCrypto::CurrencyInfo(string key)
//{
//    return config[currency_code + "-" + key];
//}
//
//void CurrencyCrypto::Initialize()
//{
//    DetermineCurve();
//    DetermineCapabilities();
//    DetermineUsable();
//
//    if (!usable)
//        return;
//    commands.SetCommands();
//    DetermineCompressed();
//    DeterminePrefixes();
//
//    locked = IsLocked();
//}
//
//void CurrencyCrypto::DetermineCurve()
//{
//    string curve_name = lowercase(CurrencyInfo("curve"));
//    if (curve_name == "ed25519")
//        curve = ED25519;
//    else if (curve_name == "curve25519")
//        curve = CURVE25519;
//    else
//        curve = SECP256K1;
//}
//
//void CurrencyCrypto::DetermineCompressed()
//{
//    string dumped_privkey = GetStringBetweenQuotes(GetAnyDumpedPrivKey());
//    vch_t secret_bytes;
//    if (!DecodeBase58Check(dumped_privkey, secret_bytes))
//    {
//        log_ << "DetermineCompressed(): Failed to decode secret\n";
//        return;
//    }
//    else
//    {
//        log_ << "size of secret bytes is " << secret_bytes.size() << "\n";
//        if (secret_bytes.size() == 34)
//            compressed = true;
//    }
//    log_ << "compressed is " << compressed << "\n";
//}
//
//std::string CurrencyCrypto::PubKeyToCurrencyAddress(Point pubkey)
//{
//    if (curve == SECP256K1)
//    {
//        CPubKey key(pubkey);
//        if (!compressed && key.IsCompressed())
//            key.Decompress();
//        CKeyID key_id = key.GetID();
//        CBitcoinAddress address;
//        address.SetData(pubkey_prefix, &key_id, 20);
//        return address.ToString();
//    }
//    std::string pubkey_to_address_command = currency->Info("pubkeytoaddress");
//    std::string command = Replace(pubkey_to_address_command, "PUBKEY", pubkey.ToString());
//
//    std::string address = currency->rpc.ExecuteCommand(command).asString();
//    return Replace(address, "\n", "");
//}
//
//CBigNum CurrencyCrypto::CurrencySecretToPrivKey(string currency_secret)
//{
//    currency_secret = GetStringBetweenQuotes(currency_secret);
//
//    vch_t secret_bytes;
//    CBigNum privkey;
//    if (not DecodeBase58Check(currency_secret, secret_bytes))
//    {
//        log_ << "Failed to decode secret\n";
//    }
//    else
//    {
//        if (secret_bytes.size() == 34)
//            secret_bytes.resize(33);
//        reverse(secret_bytes.begin() + 1, secret_bytes.end());
//
//        uint256 secret_256(vch_t(secret_bytes.begin() + 1, secret_bytes.end()));
//
//        privkey = CBigNum(secret_256);
//    }
//    return privkey;
//}
//
//string CurrencyCrypto::PrivKeyToCurrencySecret(CBigNum privkey)
//{
//    auto bytes = privkey_prefix;
//
//    log_ << "PrivateKeyToCurrencySecret: prefix is " << bytes[0] << "\n";
//    vch_t privkey_bytes = privkey.getvch32();
//
//    reverse(privkey_bytes.begin(), privkey_bytes.end());
//
//    log_ << "privkey bytes are: " << privkey_bytes << "\n";
//
//    bytes.insert(bytes.end(), privkey_bytes.begin(), privkey_bytes.end());
//
//    if (compressed)
//    {
//        log_ "adding final 1 before encoding\n";
//        vch_t byte;
//        byte.resize(1);
//        byte[0] = 1;
//        bytes.reserve(bytes.size() + 1);
//        bytes.insert(bytes.end(), byte.begin(), byte.end());
//    }
//
//    log_ "bytes are now: " << bytes << "\n";
//
//    return EncodeBase58Check(bytes);
//}
//vch_t PrefixFromBase58Data(string base58string)
//{
//    CBase58Data data;
//    data.SetString(base58string);
//    return data.vchVersion;
//}
//
//vector<Unspent> CurrencyCrypto::ExtractUnspentFromOutput(string json_output)
//{
//    vector<Unspent> unspent;
//    Json::Value list_of_outputs;
//
//    Json::Reader reader;
//
//    log_ << "extracting unspent\n";
//    if (not reader.parse(json_output, list_of_outputs, false))
//    {
//        log_ << "ExtractUnspentFromOutput: Can't parse: " << json_output << "\n";
//        return unspent;
//    }
//
//    for (uint32_t i = 0; i < list_of_outputs.size(); i++)
//    {
//        Json::Value output = list_of_outputs[i];
//        if (Contains(json_output, "scriptPubKey") and Contains(json_output, "type"))
//        {
//            Json::Value script_pubkey = output["scriptPubKey"];
//            if (script_pubkey["type"].asString() != "pubkeyhash")
//                continue;
//        }
//
//        Unspent item;
//        if (Contains(json_output, "txid"))
//            item.txid = output["txid"].asString();
//        else if (Contains(json_output, "prevout_hash"))
//            item.txid = output["prevout_hash"].asString();
//        else
//            continue;
//
//        if (Contains(json_output, "prevout_n"))
//        {
//            int n = output["prevout_n"].asInt();
//            log_ << "n = " << n << "\n";
//            item.n = (uint32_t) n;
//        }
//        else if (Contains(json_output, "vout"))
//            item.n = (uint32_t) output["vout"].asInt();
//
//        if (Contains(json_output, "amount"))
//        {
//            item.amount = (uint64_t) currency->RealToAmount(output["amount"].asDouble());
//        }
//        else if (Contains(json_output, "value"))
//        {
//            try
//            {
//                item.amount = (uint64_t) currency->RealToAmount(output["value"].asDouble());
//            }
//            catch (...)
//            {
//                string value_string = output["value"].asString();
//                value_string = Replace(value_string, "\"", "");
//                item.amount = (uint64_t) ExtractBalanceFromString(value_string);
//            }
//        }
//        else
//            continue;
//
//        if (Contains(json_output, "confirmations"))
//            item.confirmations = (uint32_t) output["confirmations"].asInt();
//        else
//            item.confirmations = 1;
//
//        item.address = output["address"].asString();
//        unspent.push_back(item);
//    }
//    return unspent;
//}
//
//CurrencyTxOut CurrencyCrypto::CurrencyTxOutFromJSON(string json)
//{
//    CurrencyTxOut txout;
//
//    Json::Value json_object;
//    Json::Reader reader;
//
//    if (not reader.parse(json, json_object))
//    {
//        std::cerr << "CurrencyTxOutFromJSON: Can't parse: "  << json << "\n";
//        return txout;
//    }
//
//    Json::Value script_pubkey = json_object["scriptPubKey"];
//    if (script_pubkey["type"].asString() != "pubkeyhash")
//        return txout;
//    Json::Value addresses = script_pubkey["addresses"];
//
//    if (addresses.size() != 1)
//        return txout;
//
//    txout.amount = (uint64_t) currency->RealToAmount(json_object["value"].asDouble());
//    txout.address = addresses[0].asString();
//    txout.confirmations = (uint32_t) json_object["confirmations"].asInt();
//    return txout;
//}
//
//void CurrencyCrypto::DeterminePrefixes()
//{
//    string address = GetAnyAddress();
//    log_ << "got address: " << address << "\n";
//
//    if (privkey_prefix.size() == 0)
//    {
//        if (address.size() == 0)
//        {
//            usable = false;
//            return;
//        }
//        pubkey_prefix = PrefixFromBase58Data(address);
//        string dumped_privkey = GetAnyDumpedPrivKey();
//        dumped_privkey = GetStringBetweenQuotes(dumped_privkey);
//
//        if (IsLocked())
//        {
//            log_ << "locked - not getting privkey prefix\n";
//            return;
//        }
//
//        if (dumped_privkey.size() == 0)
//        {
//            usable = false;
//            return;
//        }
//        privkey_prefix = PrefixFromBase58Data(dumped_privkey);
//        if (privkey_prefix.size() == 0)
//        {
//            usable = false;
//            return;
//        }
//        log_ << "pubkey prefix is " << pubkey_prefix << "\n";
//#ifndef _PRODUCTION_BUILD
//        log_ << "got prefix " << privkey_prefix[0] << " from "
//             << dumped_privkey << "\n";
//#endif
//    }
//}
//
//void CurrencyCrypto::DetermineCapabilities()
//{
//    if (not capabilities_determined)
//    {
//        string_t help_command("help");
//        string help;
//        try
//        {
//            help = rpc.ExecuteCommand(help_command).toStyledString();
//        }
//        catch (...)
//        {
//            log_ << "Encountered exception\n";
//        }
//        log_ << "output from " << help_command << " is " << help << "\n";
//
//        if (help == "")
//        {
//            log_ << "No help for " << currency_code <<  " - unusable.\n";
//            return;
//        }
//
//        string help_nospace = Replace(help, " ", "");
//
//        for (auto item : default_commands)
//        {
//            string command_name = item.first;
//            string command_string = item.second;
//            bool supported = Contains(help, command_name) and not Contains(help_nospace, command_name + "Deprecated")
//                                                          and not Contains(help_nospace, command_name + "sDeprecated");
//            log_ << "supports[" << command_name << "]=" << supported << "\n";
//            if (supported)
//                RecordSupportFor(command_name);
//        }
//    }
//}
//
//bool CurrencyCrypto::Supports(string command_name)
//{
//    return commands.Supports(command_name);
//}
//
//void CurrencyCrypto::RecordSupportFor(string command_name)
//{
//    commands.supported_commands.insert(command_name);
//}
//
//void CurrencyCrypto::DetermineUsable()
//{
//    if (capabilities_determined and not usable)
//    {
//        log_ << "already determined not to be usable\n";
//        return;
//    }
//
//    usable = true;
//
//    if (not (Supports("sendtoaddress") or Supports("payto")))
//    {
//        log_ << "No way to pay! Must support sendtoaddress or payto!\n";
//        usable = false;
//    }
//    if (not (Supports("importprivkey") or Supports("sweep") or Supports("sweepprivkey")))
//    {
//        log_ << "No way to import private keys!";
//        log_ << "Must support importprivkey, sweep or sweepprivkey!\n";
//        usable = false;
//    }
//    if (not (Supports("dumpprivkey") or Supports("getprivatekeys")))
//    {
//        log_ << "No way to export private keys!";
//        log_ << "Must support dumpprivkey or getprivatekeys!\n";
//        usable = false;
//    }
//    if (curve != SECP256K1 and currency->Info("pubkey_to_address_command") == "")
//    {
//        log_ << "Non-Secp256k1 currencies must specify pubkey_to_address_command in config!\n";
//        usable = false;
//    }
//    if (not (Supports("gettxout") or Supports("import_address") or Supports("getaddressbalance")))
//    {
//        log_ << "No way to check payments have been made!\n"
//             << "Currency must support one of:\n"
//             << "gettxout\n"
//             << "importaddress\n"
//             << "getaddressbalance\n"
//             << "or define " << currency_code
//             << "-getaddressbalance in the config file\n";
//
//        usable = false;
//    }
//    if (not usable)
//        log_ << "currency " << currency_code << " is unusable\n";
//}
//
//string CurrencyCrypto::Broadcast(string data)
//{
//    if (!Supports("broadcast"))
//        return "";
//    string command = commands.BroadcastCommand(data);
//    return rpc.ExecuteCommand(command).asString();
//}
//
//string CurrencyCrypto::AddNToTxId(string txid, Point pubkey, uint64_t amount)
//{
//    uint32_t n = 0;
//    string address = PubKeyToAddress(pubkey);
//    log_ << "looking for txout with txid " << txid
//         << " and amount " << amount << "\n";
//    while (true)
//    {
//        CurrencyTxOut txout = GetTxOut(txid, n);
//        if (!txout)
//            break;
//        log_ << "txout has amount " << txout.amount << "\n";
//        if (txout.address == address && txout.amount == amount)
//        {
//            char str_n[5];
//            sprintf(str_n, ":%u", n);
//            log_ << "n is " << n << "\n";
//            return txid + string(str_n);
//        }
//        n++;
//    }
//    log_ << "Couldn't find txout\n";
//    return "";
//}
//
//string CurrencyCrypto::SendToPubKey(Point pubkey, uint64_t amount)
//{
//    string command = commands.SendToPubKeyCommand(pubkey, amount);
//    log_ << "executing command: " << command << "\n";
//    string output = rpc.ExecuteCommand(command).asString();
//    output.erase(remove(output.begin(), output.end(), '\n'), output.end());
//    return AddNToTxId(output, pubkey, amount);
//}
//
//string CurrencyCrypto::PubKeyToAddress(Point pubkey)
//{
//    return PubKeyToCurrencyAddress(pubkey);
//}
//
//string CurrencyCrypto::PrivKeyToSecret(CBigNum privkey)
//{
//    return PrivKeyToCurrencySecret(privkey);
//}
//
//int64_t CurrencyCrypto::GetPubKeyBalance(Point pubkey)
//{
//    string command = commands.GetPubKeyBalanceCommand(pubkey);
//    string result = rpc.ExecuteCommand(command).asString();
//    return ExtractBalanceFromString(result);
//}
//
//bool CurrencyCrypto::ImportPrivateKey(CBigNum privkey)
//{
//    string command = commands.ImportPrivateKeyCommand(privkey);
//    if (command == string(""))
//        return false;
//#ifndef _PRODUCTION_BUILD
//    log_ << "private key is: " << privkey << "\n";
//#endif
//    Point pubkey(curve, privkey);
//
//    log_ << "public key is: " << pubkey << "\n";
//    string address = PubKeyToAddress(pubkey);
//    log_ << "address is " << address << "\n";
//#ifndef _PRODUCTION_BUILD
//    log_ << "executing: " << command << "\n";
//#endif
//    string result = rpc.ExecuteCommand(command).asString();
//    if (result.find(":") != string::npos)
//    {
//        if (Contains(result, "imported"))
//            return true;
//        std::cerr << "ImportCryptoCurrencyPrivateKey: failed: " << result << "\n";
//        return false;
//    }
//    return true;
//}
//
//bool CurrencyCrypto::ImportPrivateKey(string privkey_string)
//{
//    CBigNum privkey = CurrencySecretToPrivKey(privkey_string);
//    return ImportPrivateKey(privkey);
//}
//
//string CurrencyCrypto::DumpPrivKey(string address)
//{
//    string command = commands.GetPrivateKeyCommand(address);
//    string privkey = rpc.ExecuteCommand(command).asString();
//#ifndef _PRODUCTION_BUILD
//    log_ << "dump privkey command = " << command << "\n";
//    log_ << "privkey = " << privkey << "\n"
//         << "number = "
//         << CurrencySecretToPrivKey(privkey) << "\n";
//#endif
//    return privkey;
//}
//
//string CurrencyCrypto::GetAnyDumpedPrivKey()
//{
//    string address = GetAnyAddress();
//    return DumpPrivKey(address);
//}
//
//string CurrencyCrypto::GetAnyAddress()
//{
//    log_ << "trying to get unspent\n";
//    vector<Unspent> unspent_outputs = ListUnspent();
//    log_ << "got " << unspent_outputs.size() << " unspent\n";
//    if (unspent_outputs.size() > 0)
//        return unspent_outputs[0].address;
//
//    if (Supports("getnewaddress"))
//        return GetNewAddress();
//
//    if (Supports("listaddresses"))
//    {
//        vector<string> addresses = ListAddresses();
//        if (addresses.size() > 0)
//            return addresses[0];
//    }
//    string cc(currency_code.begin(), currency_code.end());
//    log_ << "Failed to get any address for " << cc << "\n";
//    return "";
//}
//
//string CurrencyCrypto::GetNewAddress()
//{
//    string command = commands.GetNewAddressCommand();
//    log_ <<"new address command: " << command << "\n";
//    return rpc.ExecuteCommand(command).asString();
//}
//
//vector<string> CurrencyCrypto::ListAddresses()
//{
//    string command = commands.ListAddressesCommand();
//    log_ << "list addresses command: " << command << "\n";
//    string list_address_output = rpc.ExecuteCommand(command).asString();
//    vector<string> addresses;
//    string address, rest;
//
//    while (Contains(list_address_output, ","))
//    {
//        split_string(list_address_output, address, rest, ",");
//        address = GetStringBetweenQuotes(address);
//        if (address.size() > 0 and !Contains(address, " "))
//            addresses.push_back(address);
//        list_address_output = rest;
//    }
//    address = GetStringBetweenQuotes(list_address_output);
//    if (address.size() > 0 and !Contains(address, " "))
//        addresses.push_back(address);
//    log_ << "addresses are " << addresses << "\n";
//    return addresses;
//}
//
//string CurrencyCrypto::MakeTradeable(uint64_t amount)
//{
//    Point pubkey(curve, GetPrivateKey(GetNewAddress()));
//    return SendToPubKey(pubkey, amount);
//}
//
//CBigNum CurrencyCrypto::GetPrivateKey(string address)
//{
//    string dumped_privkey = DumpPrivKey(address);
//    CBigNum privkey = CurrencySecretToPrivKey(dumped_privkey);
//    log_ << "privkey for " << dumped_privkey << " is "
//         << privkey << "\n";
//    log_ << "pubkey is " << Point(curve, privkey) << "\n";
//    log_ << "keyhash is " << KeyHash(Point(curve, privkey)) << "\n";
//    log_ << "address is " << PubKeyToAddress(Point(curve, privkey)) << "\n";
//    // keydata[Point(curve, privkey)]["privkey"] = privkey;
//    return privkey;
//}
//
//vector<Unspent> CurrencyCrypto::ListUnspent()
//{
//    string command = commands.ListUnspentCommand();
//    log_ << "listunspentcommand: " << command << "\n";
//    string json_output = rpc.ExecuteCommand(command).toStyledString();
//    log_ << "output: " <<  json_output << "\n";
//    return ExtractUnspentFromOutput(json_output);
//}
//
//Unspent CurrencyCrypto::GetUnspentWithAmount(uint64_t amount)
//{
//    vector<Unspent> unspent_outputs = ListUnspent();
//    for (auto unspent : unspent_outputs)
//    {
//        if (unspent.amount >= amount)
//            return unspent;
//    }
//    Unspent empty_unspent;
//    return empty_unspent;
//}
//
//CurrencyTxOut CurrencyCrypto::GetTxOut(string txid, uint32_t n)
//{
//    string command = commands.GetTxOutCommand(txid, n);
//    log_ << "GetTxOutCommand: " <<  command << "\n";
//    string json_output = rpc.ExecuteCommand(command).toStyledString();
//    log_ << "json = " << json_output << "\n";
//    CurrencyTxOut txout = CurrencyTxOutFromJSON(json_output);
//    txout.txid = txid;
//    txout.n = n;
//    return txout;
//}
//
//CurrencyTxOut CurrencyCrypto::GetTxOutWithAmount(uint64_t amount)
//{
//    log_ << "GetTxOutWithAmount: " << amount << "\n";
//    Unspent unspent = GetUnspentWithAmount(amount);
//    return GetTxOut(unspent.txid, unspent.n);
//}
//
//bool txid_and_n_from_payment_proof(vch_t payment_proof, string& txid, uint32_t& n)
//{
//    string proof(payment_proof.begin(), payment_proof.end());
//    string string_n;
//    if (!split_string(proof, txid, string_n))
//        return false;
//    n = (uint32_t) string_to_uint(string_n);
//    return true;
//}
//
//vch_t payment_proof_from_txid_and_n(string txid, uint32_t n)
//{
//    string proof = txid + ":" + ToString(n);
//    return vch_t(proof.begin(), proof.end());
//}
//bool CurrencyCrypto::VerifyPayment(string payment_proof, Point pubkey, uint64_t amount, uint32_t confirmations_required)
//{
//    string txid;
//    uint32_t n;
//    log_ << "verifying payment with proof: " << payment_proof << "\n";
//    vch_t proof(payment_proof.begin(), payment_proof.end());
//    if (!txid_and_n_from_payment_proof(proof, txid, n))
//    {
//        log_ << "couldn't get txid and n\n";
//        return false;
//    }
//
//    return VerifyPayment(txid, n, pubkey, amount, confirmations_required);
//}
//
//bool CurrencyCrypto::VerifyPayment(string txid, uint32_t n, Point pubkey, uint64_t amount,
//                                   uint32_t confirmations_required)
//{
//    log_ << "VerifyPayment(): order pubkey=" << pubkey << "\n";
//    string address = PubKeyToAddress(pubkey);
//    if (Supports("gettxout"))
//    {
//        log_ << "using gettxout: " << txid << n << "\n";
//        CurrencyTxOut txout = GetTxOut(txid, n);
//        if (txout.confirmations < confirmations_required)
//        {
//            log_ << "not enough confirmations: " << txout.confirmations << " vs " << confirmations_required << "\n";
//            return false;
//        }
//        log_ << "address check: " << txout.address << " vs " << address << "\n";
//        return txout.address == address && txout.amount >= amount;
//
//    }
//    else if (Supports("import_address"))
//    {
//        log_ << "using getpubkeybalance: " << pubkey << "\n";
//        int64_t balance = GetPubKeyBalance(pubkey);
//        return balance > 0 && (uint64_t)balance >= amount;
//    }
//    log_ << "supports neither gettxout nor import_address\n";
//    return false;
//}
//
//bool CurrencyCrypto::IsLocked()
//{
//    string command = commands.InfoCommand();
//    log_ << "commands.InfoCommand() is " << command << "\n";
//    string info_json = rpc.ExecuteCommand(command).toStyledString();
//    log_ << "IsLocked(): info_json is " << info_json << "\n";
//
//    Json::Value reported_info;
//    Json::Reader reader;
//
//    if (not reader.parse(info_json, reported_info, false))
//    {
//        std::cerr << "IsLocked(): Can't parse info string: " << info_json << "\n";
//        if (Contains(info_json, "lectrum"))
//        {
//            electrum = true;
//            return false;
//        }
//        return true;
//    }
//    Json::Value unlock_time_value = reported_info["unlocked_until"];
//
//    if (unlock_time_value.asInt() == 0)
//        locked = true;
//    else if (unlock_time_value.asInt() > GetTimeMicros() / 1e6)
//        locked = false;
//    return locked;
//}
//
//int64_t CurrencyCrypto::ExtractBalanceFromString(string input_string)
//{
//    size_t brace_location = input_string.find("{");
//    string balance_string;
//    if (brace_location == string::npos)
//        balance_string = input_string;
//    else
//    {   // electrum says {"confirmed": "xxxxxx"}
//        size_t location_of_confirmed = input_string.find("confirmed\": \"");
//        if (location_of_confirmed == string::npos)
//            return -1;
//        size_t end_of_confirmed
//                = input_string.find("\"", location_of_confirmed + 13);
//        if (end_of_confirmed == string::npos)
//            return -1;
//        balance_string = input_string.substr(location_of_confirmed + 13,
//                                             end_of_confirmed
//                                             - (location_of_confirmed + 13));
//    }
//    log_ << "balance_string is " << balance_string << "\n";
//    size_t dot_location = balance_string.find(".");
//    log_ << "dot location is " << dot_location << "\n";
//    if (dot_location == string::npos)
//        balance_string = balance_string + ".";
//
//    while (balance_string.length() <= balance_string.find(".") + currency->decimal_places)
//        balance_string = balance_string + "0";
//    log_ << "balance_string is now " << balance_string << "\n";
//    balance_string = balance_string.replace(balance_string.find("."), 1, "");
//    log_ << "balance_string is now " << balance_string << "\n";
//    while (balance_string.substr(0, 1) == "0")
//        balance_string = balance_string.replace(balance_string.find("0"), 1, "");
//    log_ << "returning " << string_to_uint(balance_string) << "\n";
//    return string_to_uint(balance_string);
//}
//
//std::string CurrencyCrypto::Help()
//{
//    std::stringstream ss;
//    ss << rpc.ExecuteCommand("help");
//    log_ << ss.str() << "\n";
//    return rpc.ExecuteCommand("help").toStyledString();
//}
