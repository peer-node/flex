#ifndef TELEPORT_CURRENCYCRYPTO_H
#define TELEPORT_CURRENCYCRYPTO_H


#include "CryptoCurrencyCommands.h"
#include "CurrencyRPC.h"
//
//int64_t ExtractBalanceFromString(std::string input_string, uint32_t decimal_places);
//
//class Unspent
//{
//public:
//    std::string txid;
//    uint32_t n;
//    std::string script_pubkey;
//    std::string address;
//    uint64_t amount;
//    uint32_t confirmations;
//
//    Unspent():
//        amount(0)
//    { }
//
//    bool operator!()
//    {
//        return amount == 0;
//    }
//};
//
//class CurrencyTxOut
//{
//public:
//    std::string txid;
//    uint32_t n;
//    std::string address;
//    uint64_t amount;
//    uint32_t confirmations;
//
//    CurrencyTxOut():
//        amount(0)
//    { }
//
//    bool operator!()
//    {
//        return amount == 0;
//    }
//};
//
//class Currency;
//
//class CurrencyCrypto
//{
//public:
//    Currency *currency{NULL};
//    uint8_t curve;
//    std::string currency_code;
//    CurrencyRPC rpc;
//    CryptoCurrencyCommands commands;
//    TeleportConfig &config;
//
//    vch_t privkey_prefix;
//    vch_t pubkey_prefix;
//
//    bool compressed{false};
//    bool usable{false};
//    bool locked{false};
//    bool electrum{false};
//    bool capabilities_determined{false};
//
//    CurrencyCrypto(std::string currency_code, TeleportConfig &config);
//
//    void SetCurrency(Currency *currency_)
//    {
//        currency = currency_;
//    }
//
//    void Initialize();
//
//    bool Supports(string_t command);
//
//    void DetermineCurve();
//
//    void DetermineCompressed();
//
//    void DeterminePrefixes();
//
//    void DetermineCapabilities();
//
//    void DetermineUsable();
//
//    std::string Broadcast(std::string data);
//
//    std::string PubKeyToAddress(Point pubkey);
//
//    std::string PrivKeyToSecret(CBigNum privkey);
//
//    int64_t GetPubKeyBalance(Point pubkey);
//
//    bool ImportPrivateKey(CBigNum privkey);
//
//    bool ImportPrivateKey(std::string privkey_string);
//
//    std::string DumpPrivKey(std::string address);
//
//    std::string GetAnyDumpedPrivKey();
//
//    std::string GetAnyAddress();
//
//    std::string GetNewAddress();
//
//    std::vector<std::string> ListAddresses();
//
//    std::string MakeTradeable(uint64_t amount);
//
//    CBigNum GetPrivateKey(std::string address);
//
//    std::string AddNToTxId(std::string txid, Point pubkey, uint64_t amount);
//
//    std::string SendToPubKey(Point pubkey, uint64_t amount);
//
//    std::vector<Unspent> ListUnspent();
//
//    Unspent GetUnspentWithAmount(uint64_t amount);
//
//    CurrencyTxOut GetTxOut(std::string txid, uint32_t n);
//
//    CurrencyTxOut GetTxOutWithAmount(uint64_t amount);
//
//    bool VerifyPayment(std::string payment_proof, Point pubkey, uint64_t amount, uint32_t confirmations_required);
//
//    bool VerifyPayment(std::string txid, uint32_t n, Point pubkey, uint64_t amount, uint32_t confirmations_required);
//
//    bool IsLocked();
//
//    std::string Help();
//
//    std::string CurrencyInfo(std::string key);
//
//    std::string PubKeyToCurrencyAddress(Point pubkey);
//
//    CBigNum CurrencySecretToPrivKey(std::string currency_secret);
//
//    std::string PrivKeyToCurrencySecret(CBigNum privkey);
//
//    std::vector<Unspent> ExtractUnspentFromOutput(std::string json_output);
//
//    int64_t ExtractBalanceFromString(std::string input_string);
//
//    void RecordSupportFor(std::string command_name);
//
//    CurrencyTxOut CurrencyTxOutFromJSON(std::string json);
//};
//

#endif //TELEPORT_CURRENCYCRYPTO_H
