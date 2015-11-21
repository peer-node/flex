// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "rpc/rpcserver.h"
#include "base/ui_interface.h"
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/shared_ptr.hpp>
#include "json/json_spirit_writer_template.h"

#include "rpc/rpcobjects.h"
#include "flexnode/flexnode.h"

#include "log.h"
#define LOG_CATEGORY "rpcserver.cpp"


using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace json_spirit;


static std::string strRPCUserColonPass;

// These are created by StartRPCThreads, destroyed in StopRPCThreads
static asio::io_service* rpc_io_service = NULL;
static map<string, boost::shared_ptr<deadline_timer> > deadlineTimers;
static ssl::context* rpc_ssl_context = NULL;
static boost::thread_group* rpc_worker_group = NULL;
static boost::asio::io_service::work *rpc_dummy_work = NULL;
static std::vector< boost::shared_ptr<ip::tcp::acceptor> > rpc_acceptors;

void RPCTypeCheck(const Array& params,
                  const list<Value_type>& typesExpected,
                  bool fAllowNull)
{
    unsigned int i = 0;
    BOOST_FOREACH(Value_type t, typesExpected)
    {
        if (params.size() <= i)
            break;

        const Value& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s, got %s",
                                   Value_type_name[t], Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheck(const Object& o,
                  const map<string, Value_type>& typesExpected,
                  bool fAllowNull)
{
    BOOST_FOREACH(const PAIRTYPE(string, Value_type)& t, typesExpected)
    {
        const Value& v = find_value(o, t.first);
        if (!fAllowNull && v.type() == null_type)
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first));

        if (!((v.type() == t.second) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s for %s, got %s",
                                   Value_type_name[t.second], t.first, Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }
}

int64_t AmountFromValue(const Value& value)
{
    double dAmount = value.get_real();
    if (dAmount <= 0.0 || dAmount > 21000000.0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    int64_t nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

Value ValueFromAmount(int64_t amount)
{
    return (double)amount / (double)COIN;
}

std::string HexBits(unsigned int nBits)
{
    union {
        int32_t nBits;
        char cBits[4];
    } uBits;
    uBits.nBits = htonl((int32_t)nBits);
    return HexStr(BEGIN(uBits.cBits), END(uBits.cBits));
}

uint256 ParseHashV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    uint256 result;
    result.SetHex(strHex);
    return result;
}
uint256 ParseHashO(const Object& o, string strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}
vector<unsigned char> ParseHexV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    return ParseHex(strHex);
}
vector<unsigned char> ParseHexO(const Object& o, string strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}


///
/// Note: This interface may still be subject to change.
///

string CRPCTable::help(string strCommand) const
{
    log_ << "help!\n";
    string strRet;
    set<rpcfn_type> setDone;
    for (map<string, const CRPCCommand*>::const_iterator mi = mapCommands.begin(); mi != mapCommands.end(); ++mi)
    {
        const CRPCCommand *pcmd = mi->second;
        string strMethod = mi->first;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod.find("label") != string::npos)
            continue;
        if (strCommand != "" && strMethod != strCommand)
            continue;

        try
        {
            Array params;
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
                (*pfn)(params, true);
        }
        catch (std::exception& e)
        {
            // Help text is returned in an exception
            string strHelp = string(e.what());
            if (strCommand == "")
                if (strHelp.find('\n') != string::npos)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));
            strRet += strHelp + "\n";
        }
    }
    if (strRet == "")
        strRet = strprintf("help: unknown command: %s\n", strCommand);
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}

Value help(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "help ( \"command\" )\n"
            "\nList all commands, or get help for a specified command.\n"
            "\nArguments:\n"
            "1. \"command\"     (string, optional) The command to get help on\n"
            "\nResult:\n"
            "\"text\"     (string) The help text\n"
        );

    string strCommand;
    if (params.size() > 0)
        strCommand = params[0].get_str();

    return tableRPC.help(strCommand);
}


Value stop(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "stop\n"
            "\nStop Flex server.");
    // Shutdown will take long enough that the response should get back
    StartShutdown();
    return "Flex server stopping";
}

Value getsalt(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getsalt\n"
            "\nGet the salt which, combined with your passphrase, "
            "deterministically\ngenerates your addresses and private keys.\n"
            "\nResult:\n"
            "\"salt\"     (string) The base58-encoded salt\n"
        );
    return EncodeBase58Check(GetSalt());
}

Value setsalt(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "setsalt <salt>\n"
            "\nGet the salt which, combined with your passphrase, "
            "deterministically\ngenerates your addresses and private keys.\n"
            "\nArguments:\n"
            "1. \"salt\"     (string) base58-encoded salt\n"
        );
    vch_t salt;
    if (!DecodeBase58Check(params[0].get_str(), salt))
        throw runtime_error("invalid salt\n");
    SetSalt(salt);
    flexnode.ClearEncryptionKeys();
    flexnode.InitializeWalletAndEncryptedDatabases(GetArg("-walletpassword",
                                                          "no passphrase"));
    return "";
}

Value placeorder(const Array& params, bool fHelp)
{   
    if (fHelp || params.size() < 4 || params.size() > 5)
        throw runtime_error(
            "placeorder side currency size price "
            "(work_time)\n"
            "where:\n"
            " - side is bid or ask (buy and sell work as well)\n"
            " - currency is BTC, LTC or some other cryptocurrency\n"
            " - size is the number of units of the currency\n"
            " - price is the number of FLX you are\n"
            "   willing to pay or accept for 1 unit of the currency\n"
            "\n"
            "     e.g. to buy 0.01 BTC for 0.02 FLX:\n"
            "      placeorder bid BTC 0.01 2\n"
            "\n"
            " - You can optionally specify work_time (e.g. 200), in\n"
            "   which case a proof of work, requiring work_time\n"
            "   milliseconds on average to compute, will be added\n"
            "   to the order. In times of high bandwidth usage,\n"
            "   orders with more work will be propagated more \n"
            "   quickly throughout the network."
            " - If the currency you wish to trade is not a cryptocurrency\n"
            "   then you will need to specify additional data\n"
            "   such as an account number or email address to enable\n"
            "   the payment to be made. This data should be specified\n"
            "   in the flex.conf file as XYZ-accountdata, where XYZ is\n"
            "   the currency code.\n"
            );
    string side = params[0].get_str();
    string currency = params[1].get_str();
    double size_double = params[2].get_real();
    
    double price = params[3].get_real();
    uint64_t integer_price = roundint64(price * 1e8);

    vch_t currency_code(currency.begin(), currency.end());

    if (!flexnode.currencies.count(currency_code))
        throw runtime_error("No such currency!");

    Currency cur = flexnode.currencies[currency_code];
    uint64_t size = cur.RealToAmount(size_double, currency_code);

    uint8_t side_byte;
    if (side == "buy" || side == "bid")
        side_byte = BID;
    if (side == "sell" || side == "ask")
        side_byte = ASK;

    uint64_t difficulty = 0;

    if (params.size() > 4)
    {
        uint64_t work_time = params[4].get_int();
        difficulty = work_time * 1000000;
    }

    string_t auxiliary_data = CurrencyInfo(currency_code, "accountdata");

    uint160 order_hash = PlaceOrder(currency_code,
                                    side_byte, 
                                    size, 
                                    integer_price,
                                    difficulty,
                                    auxiliary_data);
    if (order_hash == 0)
        return string("Failed to place order!");
    return order_hash.ToString();
}

Value listmyorders(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("listmyorders ( currency_code )\n");

    vch_t currency_code;
    if (params.size() == 1)
    {
        string currency = params[0].get_str();
        currency_code = vch_t(currency.begin(), currency.end());
    }

    Array orders;

    uint64_t start_time = initdata["instance"]["start_time"];
    CLocationIterator<> order_scanner;
    order_scanner = tradedata.LocationIterator("my_orders");
    uint160 order_hash, accept_commit_hash;
    uint64_t order_time;

    order_scanner.Seek(start_time);
    log_ << "listmyorders: starting order scan from " << start_time << "\n";
    while (order_scanner.GetNextObjectAndLocation(order_hash, order_time))
    {
        log_ << "found order " << order_hash << " at " << order_time << "\n";
        Order order = msgdata[order_hash]["order"];
        log_ << "order is " << order << "\n";
        if (currency_code.size() > 0 && order.currency != currency_code)
            continue;
        Object order_object = GetObjectFromOrder(order);
        order_object.push_back(Pair("hash", order_hash.ToString()));
        order_object.push_back(Pair("status", OrderStatus(order_hash)));

        if (tradedata[order_hash].HasProperty("accept_commit_hash"))
        {
            accept_commit_hash = msgdata[order_hash]["accept_commit_hash"];
            order_object.push_back(Pair("accept_commit_hash",
                                        accept_commit_hash.ToString()));
        }
        orders.push_back(order_object);
    }
    return orders;
}


Value listmytrades(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("listmytrades ( currency_code )\n");
    vch_t currency_code;
    if (params.size() == 1)
    {
        string currency = params[0].get_str();
        currency_code = vch_t(currency.begin(), currency.end());
    }
    Array trades;

    uint64_t start_time = initdata["instance"]["start_time"];
    CLocationIterator<> trade_scanner;
    trade_scanner = tradedata.LocationIterator("my_trades");
    uint160 order_hash, accept_commit_hash;
    uint64_t trade_time;

    trade_scanner.Seek(start_time);
    AcceptCommit accept_commit;
    while (trade_scanner.GetNextObjectAndLocation(accept_commit_hash, 
                                                  trade_time))
    {
        Object trade_object = GetObjectFromTrade(accept_commit_hash);
        trade_object.push_back(Pair("time", trade_time));
        trades.push_back(trade_object);
    }
    return trades;
}

Value listtrades(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error("listtrades <currency_code> [start] [end]\n");
    
    Array trades;

    uint64_t start = 0, end = 1ULL << 63;
    if (params.size() > 1)
        start = params[1].get_int();
    if (params.size() > 2)
        end = params[2].get_int();

    string currency = params[0].get_str();
    vch_t currency_code = vch_t(currency.begin(), currency.end());
    CLocationIterator<vch_t> trade_scanner;
    trade_scanner = marketdata.LocationIterator(currency_code);
    trade_scanner.Seek(start);
    uint160 accept_commit_hash;
    uint64_t trade_time;

    while (trade_scanner.GetNextObjectAndLocation(accept_commit_hash, 
                                                  trade_time))
    {
        if (trade_time > end)
        {
            log_ << "listtrades: reached past end " << end 
                 << " at " << trade_time << "\n";
            break;
        }
        Object trade_object = GetObjectFromTrade(accept_commit_hash);
        trade_object.push_back(Pair("time", trade_time));
        trades.push_back(trade_object);
    }
    return trades;
}

Array GetArrayFromStubs(vector<OrderStub> stubs)
{
    Array array;
    foreach_(const OrderStub& stub, stubs)
    {
        Object order;
        order.push_back(Pair("size", stub.size));
        order.push_back(Pair("price", stub.integer_price / 1e8));
        order.push_back(Pair("id", stub.order_hash.ToString()));
        array.push_back(order);
    }
    return array;
}

Value book(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("book <currency_code>\n");
    
    string currency = params[0].get_str();
    
    vch_t currency_code(currency.begin(), currency.end());
    Book book(currency_code);

    Object book_object;
    Array bids = GetArrayFromStubs(book.bids);
    Array asks = GetArrayFromStubs(book.asks);
    
    book_object.push_back(Pair("bids", bids));
    book_object.push_back(Pair("asks", asks));

    return book_object;
}

Value getorder(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getorder <order_id>\n");
    string order_hash_string = params[0].get_str();
    uint160 order_hash(order_hash_string);
    
    Order order = marketdata[order_hash]["order"];
    if (!order)
        return string("No such order");
    
    return GetObjectFromOrder(order);
}

Value cancelorder(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("cancelorder <order_id>\n");
    string order_hash_string = params[0].get_str();
    uint160 order_hash(order_hash_string);

    Order order = marketdata[order_hash]["order"];
    if (!order)
        return string("No such order");

    if (!keydata[order.VerificationKey()].HasProperty("privkey"))
        return string("Not my order");
    
    CancelOrder cancel(order_hash);
    flexnode.tradehandler.BroadcastMessage(cancel);
    
    return cancel.GetHash160().ToString();
}

Value acceptorder(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "acceptorder <order_id> [work_time]\n");
    string order_hash_string = params[0].get_str();
    uint160 order_hash(order_hash_string);
    Order order = marketdata[order_hash]["order"];
    if (!order)
        return string("No such order");

    string_t auxiliary_data = CurrencyInfo(order.currency, "accountdata");

   uint64_t difficulty = 0;

    if (params.size() > 1)
    {
        uint64_t work_time = params[1].get_int();
        difficulty = work_time * 1000000;
    }

    uint160 accept_hash = SendAccept(order, difficulty, auxiliary_data);
    if (accept_hash == 0)
        return string("Failed to accept order!");
    return accept_hash.ToString();
}

Value confirmfiatpayment(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("confirmfiatpayment <accept_commit_hash>\n");
    string accept_commit_hash_string = params[0].get_str();
    uint160 accept_commit_hash(accept_commit_hash_string);
    uint160 acknowledgement_hash
        = tradedata[accept_commit_hash]["acknowledgement_hash"];

    if (acknowledgement_hash == 0)
        throw runtime_error("Acknowledgement was never received!\n");

    ThirdPartyPaymentConfirmation confirmation(acknowledgement_hash);
    flexnode.tradehandler.BroadcastMessage(confirmation);
    return confirmation.GetHash160().ToString();
}

Currency CurrencyFromParam(Value param)
{
    string currency_string = param.get_str();
    vch_t currency_code(currency_string.begin(), currency_string.end());
    Currency currency = flexnode.currencies[currency_code];
    return currency;
}

Value listcurrencies(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error("listcurrencies\n");

    Array currency_list;
    for (std::map<vch_t,Currency>::iterator it = flexnode.currencies.begin(); 
         it != flexnode.currencies.end(); ++it)
    {
        log_ << string(it->first.begin(), it->first.end()) << "\n";
        currency_list.push_back(string(it->first.begin(), it->first.end()));
    }
    return currency_list;
}

Value currencygetbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "currencygetbalance <currency_code>\n");
    Currency currency = CurrencyFromParam(params[0]);
    return AmountToString(currency.Balance(), currency.currency_code);
}

Value currencygettradeable(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "currencygettradeable <currency_code>\n");
    Currency currency = CurrencyFromParam(params[0]);
    return AmountToString(currency.Balance(), currency.currency_code);
}

Value currencylistunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "currencylistunspent <currency_code>\n");
    Currency currency = CurrencyFromParam(params[0]);
    vector<Unspent> unspent_list = currency.crypto.ListUnspent();
    Array unspent_array;

    foreach_(const Unspent& unspent, unspent_list)
    {
        Object unspent_object;
        unspent_object.push_back(Pair("txid", unspent.txid));
        unspent_object.push_back(Pair("n", (uint64_t)unspent.n));
        unspent_object.push_back(Pair("amount", unspent.amount));
        unspent_object.push_back(Pair("confirmations", 
                                      (uint64_t)unspent.confirmations));
        unspent_object.push_back(Pair("address", unspent.address));
        unspent_array.push_back(unspent_object);
    }
    return unspent_array;
}


Value currencysendtoaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "currencysendtoaddress <currency_code> <address> <amount>\n");
    Currency currency = CurrencyFromParam(params[0]);
    string address = params[1].get_str();
    uint64_t amount = params[2].get_int();
    return currency.SendToAddress(address, amount);
}

Value currencygettxout(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "currencygettxout <currency_code> <txid> <n>\n");
    Currency currency = CurrencyFromParam(params[0]);
    string txid = params[1].get_str();
    uint32_t n = params[2].get_int();
    CurrencyTxOut txout = currency.crypto.GetTxOut(txid, n);
    Object txout_object;
    txout_object.push_back(Pair("txid", txout.txid));
    txout_object.push_back(Pair("n", (uint64_t)txout.n));
    txout_object.push_back(Pair("address", txout.address));
    txout_object.push_back(Pair("amount", txout.amount));
    txout_object.push_back(Pair("confirmations", 
                                (uint64_t)txout.confirmations));

    return txout_object;
}

Value getbatch(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getbatch <mined_credit_hash>\n");

    uint160 credit_hash(params[0].get_str());
    if (!creditdata[credit_hash].HasProperty("msg"))
        throw runtime_error("MinedCredit hash not found");

    MinedCreditMessage msg = creditdata[credit_hash]["msg"];
    CreditBatch batch = ReconstructBatch(msg);

    return GetObjectFromBatch(batch);
}

Value getcalendar(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("getcalendar [mined_credit_hash]\n");

    uint160 credit_hash;

    if (params.size() == 1)
        credit_hash = uint160(params[0].get_str());
    else
        credit_hash = flexnode.previous_mined_credit_hash;

    Calendar calendar(credit_hash);
    return GetObjectFromCalendar(calendar);
}

Value getminedcredit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getminedcredit <mined_credit_hash>\n");

    uint160 credit_hash(params[0].get_str());
    if (!creditdata[credit_hash].HasProperty("mined_credit"))
        throw runtime_error("MinedCredit not found!");

    MinedCredit credit = creditdata[credit_hash]["mined_credit"];

    return GetObjectFromMinedCredit(credit);
}

Value getminedcreditmsg(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getminedcreditmsg <mined_credit_hash>\n");
    uint160 credit_hash(params[0].get_str());

    if (!creditdata[credit_hash].HasProperty("msg"))
        throw runtime_error("MinedCreditMessage not found!");

    MinedCreditMessage msg = creditdata[credit_hash]["msg"];
    return GetObjectFromMinedCreditMessage(msg);
}

Value gettransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("gettransaction <transaction_hash>\n");
    uint160 transaction_hash(params[0].get_str());
    if (!creditdata[transaction_hash].HasProperty("tx"))
        return Object();
    SignedTransaction tx = creditdata[transaction_hash]["tx"];
    return GetObjectFromTransaction(tx);
}

Value getspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() == 0)
        throw runtime_error("getspent <credit_number> [<credit_number> ...]\n");

    Array results;
    for (uint32_t i = 0; i < params.size(); i++)
    {
        uint64_t credit_number = strtoull(params[i].get_str().c_str(), 
                                          NULL, 10);
        results.push_back(flexnode.spent_chain.Get(credit_number));
    }
    return results;
}

Value getspentchain(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("getspentchain [<start>] [<end>]\n");

    uint64_t start = 0;
    uint64_t end = (1ULL << 63);

    if (params.size() > 0)
        start = strtoull(params[0].get_str().c_str(), NULL, 10);
    if (params.size() > 1)
        end = strtoull(params[1].get_str().c_str(), NULL, 10);

    BitChain spent_chain = flexnode.spent_chain;

    string result;
    for (uint64_t i = start; i < spent_chain.length && i < end; i++)
        result += spent_chain.Get(i) ? "1" : "0";

    return result;
}

Value listreceivedcredits(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("listreceivedcredits <keyhash>\n");
    uint160 keyhash(params[0].get_str());
    
    vector<uint32_t> stubs;
    stubs.push_back(GetStub(keyhash));
    vector<CreditInBatch> credits = GetCreditsPayingToRecipient(stubs, 0);
    
    Array results;
    foreach_(CreditInBatch& credit, credits)
    {
        if (credit.KeyHash() == keyhash)
            results.push_back(GetObjectFromCreditInBatch(credit));
    }
    
    return results;
}

Value getrelaystate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getrelaystate <mined_credit_hash>\n");
    uint160 credit_hash(params[0].get_str());

    RelayState state = GetRelayState(credit_hash);
    return GetObjectFromRelayState(state);
}

Value getjoin(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getjoin <join_hash>\n");
    uint160 join_hash(params[0].get_str());

    RelayJoinMessage join = msgdata[join_hash]["join"];
    return GetObjectFromJoin(join);
}

Value getsuccession(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getsuccession <succession_hash>\n");
    uint160 succession_hash(params[0].get_str());

    SuccessionMessage succession = msgdata[succession_hash]["succession"];
    return GetObjectFromSuccession(succession);
}

Value getconfirmationkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("getthirdpartyconfirmationkey\n");
    Point confirmation_key = flexnode.wallet.NewKey();
    return EncodeBase58Check(confirmation_key.getvch());
}

Value currencyimportprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "currencyimportprivkey <currency_code> <privkey>\n");
    Currency currency = CurrencyFromParam(params[0]);
    string privkey_string = params[1].get_str();
    return currency.crypto.ImportPrivateKey(privkey_string);
}

CreditInBatch CreditInBatchFromProof(const vch_t proof)
{
    CDataStream ss((const char *)&proof[0], (const char *)&proof[proof.size()],
                    SER_NETWORK, CLIENT_VERSION);
    CreditInBatch credit;
    ss >> credit;
    return credit;
}

Value verifyfunds(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("verifyfunds order_id [confirmations=1]\n");
    string order_hash_string = params[0].get_str();
    
    uint64_t confirmations = 1;
    if (params.size() > 1)
        confirmations = params[1].get_int();
    
    uint160 order_hash(order_hash_string);
    
    Order order = marketdata[order_hash]["order"];
    if (!order)
        return string("No such order");
    Currency currency = flexnode.currencies[order.currency];
    uint64_t amount_required = (order.side == BID)
                               ? order.size * order.integer_price 
                                                / 100000000
                               : order.size;
    string_t proof(order.fund_proof.begin(), order.fund_proof.end());
    
    if (order.side == ASK)
    {
        return currency.crypto.VerifyPayment(proof, order.fund_address_pubkey,
                                             amount_required, confirmations);
    }
    
    CreditInBatch credit = CreditInBatchFromProof(order.fund_proof);
 
    if (flexnode.spent_chain.Get(credit.position))
    {
        return false;
    }
    return CreditInBatchHasValidConnectionToCalendar(credit);
}

Value requestdepositaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("requestdepositaddress <currency_code>\n");

    string currency_ = params[0].get_str();
    vch_t currency_code(currency_.begin(), currency_.end());
    uint8_t curve;

    if (currency_ != "FLX")
    {
        Currency currency = flexnode.currencies[currency_code];
        curve = currency.crypto.curve;
    }
    else
    {
        curve = SECP256K1;
    }
    
    flexnode.deposit_handler.SendDepositAddressRequest(currency_code,
                                                       curve,
                                                       false);

    return "";
}

Value requestprivateaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("requestprivateaddress <currency_code>\n");

    string currency_ = params[0].get_str();
    vch_t currency_code(currency_.begin(), currency_.end());
    uint8_t curve;

    if (currency_ != "FLX")
    {
        Currency currency = flexnode.currencies[currency_code];
        curve = currency.crypto.curve;
    }
    else
    {
        curve = SECP256K1;
    }
    
    flexnode.deposit_handler.SendDepositAddressRequest(currency_code,
                                                       curve,
                                                       true);

    return "";
}

Value listdepositaddresses(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("listdepositaddresses <currency_code>\n");

    string currency_ = params[0].get_str();
    vch_t currency_code(currency_.begin(), currency_.end());

    Array addresses;

    Currency currency = flexnode.currencies[currency_code];

    vector<Point> my_addresses = depositdata[currency_code]["addresses"];
    
    foreach_(Point address, my_addresses)
    {
        bool private_ = false;
        if (depositdata[address].HasProperty("offset_point"))
        {
            private_ = true;
            log_ << "address before adding offset point is "
                 << address << "\n";
            Point offset_point = depositdata[address]["offset_point"];
            log_ << "offset_point is " << offset_point << "\n";
            address += offset_point;
        }
        log_ << "listdepositaddresses: point address is " << address << "\n";
        log_ << "address hash is " << KeyHash(address) << "\n";
        string address_string;
        if (currency_code == FLX)
            address_string = FlexAddressFromPubKey(address);
        else
            address_string = currency.crypto.PubKeyToAddress(address);
        log_ << "pubkeytoaddress is " << address_string;

        CBitcoinAddress bitcoin_address(address_string);
        CKeyID keyid;
        bitcoin_address.GetKeyID(keyid);
        vch_t bytes(BEGIN(keyid), END(keyid));

        uint160 hash_from_keyid(bytes);
        log_ << "getkeyid is " << hash_from_keyid << "\n";

        int64_t balance;
        if (currency_code == FLX)
            balance = GetKnownPubKeyBalance(address);
        else
            balance = currency.crypto.GetPubKeyBalance(address);
                                                  
        Object address_object;
        address_object.push_back(Pair("address", address_string));

        string balance_string = AmountToString(balance, currency_code);
        address_object.push_back(Pair("balance", balance_string));

        if (private_)
            address_object.push_back(Pair("private", true));

        addresses.push_back(address_object);
    }

    return addresses;
}

Value transferdeposit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "transferdeposit <deposit_address> <recipient_address>\n");

    Point address, recipient_key;

    CBitcoinAddress address_base58(params[0].get_str());
    CKeyID keyid;

    address_base58.GetKeyID(keyid);
    uint160 address_hash(vch_t(BEGIN(keyid), END(keyid)));

    if (!depositdata[address_hash]["confirmed"])
        throw runtime_error("Can't transfer until address "
                            "secrets have been disclosed");

    address = keydata[address_hash]["pubkey"];

    log_ << "transferdeposit: address is " << address << "\n";
    log_ << "has public address: "
         << depositdata[address].HasProperty("public_address") << "\n";


    if (depositdata[address].HasProperty("public_address"))
        throw runtime_error("Can't transfer private addresses using "
                            "transferdeposit. "
                            "Use transferprivateaddress.\n");

    CBitcoinAddress recipient_base58(params[1].get_str());
    recipient_base58.GetKeyID(keyid);
    uint160 recipient_key_hash(vch_t(BEGIN(keyid), END(keyid)));

    log_ << "recipient " << params[1].get_str() << " has keyhash: "
         << recipient_key_hash << "\n";

    log_ << "have pubkey: "
         << keydata[recipient_key_hash].HasProperty("pubkey") << "\n";

    DepositTransferMessage transfer(address_hash, recipient_key_hash);

    transfer.Sign();

    uint160 transfer_hash = transfer.GetHash160();

    depositdata[transfer_hash]["is_mine"] = true;
    flexnode.deposit_handler.PushDirectlyToPeers(transfer);

    return transfer_hash.ToString();
}

uint160 GetAddressHashFromPrivateAddressHash(uint160 secret_address_hash)
{
    Point secret_address, public_address;
    secret_address = depositdata[secret_address_hash]["address"];
    public_address = depositdata[secret_address]["public_address"];

    if (secret_address_hash == KeyHash(secret_address))
        return KeyHash(public_address);
    else if (secret_address_hash == FullKeyHash(secret_address))
        return FullKeyHash(public_address);
    return 0;
}

Value transferprivateaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "transferprivateaddress <deposit_address> <recipient_pubkey>\n");

    Point secret_address, recipient_key;
    uint160 address_hash;
    CBitcoinAddress address_base58(params[0].get_str());
    CKeyID keyid;

    address_base58.GetKeyID(keyid);
    uint160 secret_address_hash(vch_t(BEGIN(keyid), END(keyid)));

    if (!depositdata[secret_address_hash]["confirmed"])
        throw runtime_error("Can't transfer until address "
                            "secrets have been disclosed");

    address_hash = GetAddressHashFromPrivateAddressHash(secret_address_hash);

    string recipient_pubkey_string = params[1].get_str();

    if (recipient_pubkey_string.size() < 36)
        throw runtime_error("Must specify recipient pubkey, "
                            "not recipient address.");
    
    string key_hex = params[1].get_str();
    vch_t key_bytes = ParseHex(key_hex);
    recipient_key.setvch(key_bytes);
    
    log_ << "recipient has key: " << recipient_key << "\n";

    log_ << "have privkey: "
         << keydata[recipient_key].HasProperty("privkey") << "\n";

    DepositTransferMessage transfer(address_hash, recipient_key);

    transfer.Sign();

    uint160 transfer_hash = transfer.GetHash160();

    depositdata[transfer_hash]["is_mine"] = true;
    flexnode.deposit_handler.PushDirectlyToPeers(transfer);

    return transfer_hash.ToString();
}

Value getpubkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getpubkey <address>\n");

    Point address;
    Point recipient_key;

    CBitcoinAddress address_base58(params[0].get_str());
    CKeyID keyid;

    address_base58.GetKeyID(keyid);
    uint160 address_hash(vch_t(BEGIN(keyid), END(keyid)));

    Point pubkey = keydata[address_hash]["pubkey"];

    vch_t pubkey_bytes = pubkey.getvch();
    string bytes(pubkey_bytes.begin(), pubkey_bytes.end());
    return string_to_hex(bytes);
}

Value withdrawdeposit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() == 0 || params.size() >  2)
        throw runtime_error("withdrawdeposit <address> [<recipient_key>]\n");

    Point address;
    Point recipient_key;

    CBitcoinAddress address_base58(params[0].get_str());
    CKeyID keyid;

    address_base58.GetKeyID(keyid);
    uint160 address_hash(vch_t(BEGIN(keyid), END(keyid)));

    log_ << "withdrawdeposit: address hash is " << address_hash << "\n";

    address = depositdata[address_hash]["address"];

    if (depositdata[address].HasProperty("public_address"))
    {
        log_ << "address " << address << " is secret; getting public address\n";
        address_hash = GetAddressHashFromPrivateAddressHash(address_hash);
        log_ << "address has is now " << address_hash << "\n";
    }

    if (params.size() > 1)
    {
        string key_hex = params[1].get_str();
        vch_t key_bytes = ParseHex(key_hex);
        recipient_key.setvch(key_bytes);
    }

    log_ << "withdrawdeposit: address hash is " << address_hash << "\n"
         << " and recipient_key is " << recipient_key << "\n";

    if (!depositdata[address_hash]["confirmed"])
        throw runtime_error("Can't withdraw until address "
                            "secrets have been disclosed");
    WithdrawalRequestMessage request(address_hash, recipient_key);

    request.Sign();

    uint160 request_hash = request.GetHash160();

    depositdata[request_hash]["is_mine"] = true;
    flexnode.deposit_handler.BroadcastMessage(request);

    return request_hash.ToString();
}




//
// Call Table
//


static const CRPCCommand vRPCCommands[] =
{ //  name                      actor (function)         okSafeMode threadSafe reqWallet
  //  ------------------------  -----------------------  ---------- ---------- ---------
    /* Overall control/query calls */
    { "getinfo",                &getinfo,                true,      false,      false }, /* uses wallet if enabled */
    { "help",                   &help,                   true,      true,       false },
    { "stop",                   &stop,                   true,      true,       false },

    /* P2P networking */
    { "getnetworkinfo",         &getnetworkinfo,         true,      false,      false },
    { "addnode",                &addnode,                true,      true,       false },
    { "getaddednodeinfo",       &getaddednodeinfo,       true,      true,       false },
    { "getconnectioncount",     &getconnectioncount,     true,      false,      false },
    { "getnettotals",           &getnettotals,           true,      true,       false },
    { "getpeerinfo",            &getpeerinfo,            true,      false,      false },
    { "ping",                   &ping,                   true,      false,      false },
    { "startdownloading",       &startdownloading,       true,      false,      false },

    /* Currencies */
    { "listcurrencies",         &listcurrencies,         true,      true,       false },
    { "currencygetbalance",     &currencygetbalance,     true,      true,       false },
    { "currencygettradeable",   &currencygettradeable,   true,      true,       false },
    { "currencylistunspent",    &currencylistunspent,    true,      true,       false },
    { "currencysendtoaddress",  &currencysendtoaddress,  true,      true,       false },
    { "currencygettxout",       &currencygettxout,       true,      true,       false },
    { "currencyimportprivkey",  &currencyimportprivkey,  true,      true,       false },

    /* Data */
    { "gettransaction",         &gettransaction,         true,      true,       false },
    { "getspent",               &getspent,               true,      true,       false },
    { "getspentchain",          &getspentchain,          true,      true,       false },
    { "getbatch",               &getbatch,               true,      true,       false },
    { "getpubkey",              &getpubkey,              true,      true,       false },
    { "getcalendar",            &getcalendar,            true,      true,       false },
    { "getminedcredit",         &getminedcredit,         true,      true,       false },
    { "getminedcreditmsg",      &getminedcreditmsg,      true,      true,       false },
    { "listreceivedcredits",    &listreceivedcredits,    true,      true,       false },
    { "getrelaystate",          &getrelaystate,          true,      true,       false },
    { "getjoin",                &getjoin,                true,      true,       false },
    { "getsuccession",          &getsuccession,          true,      true,       false },

    /* Trade */
    { "placeorder",             &placeorder,             true,      true,       false },
    { "book",                   &book,                   true,      true,       false },
    { "acceptorder",            &acceptorder,            true,      true,       false },
    { "cancelorder",            &cancelorder,            true,      true,       false },
    { "getorder",               &getorder,               true,      true,       false },
    { "verifyfunds",            &verifyfunds,            true,      true,       false },
    { "listmyorders",           &listmyorders,           true,      true,       false },
    { "listmytrades",           &listmytrades,           true,      true,       false },
    { "listtrades",             &listtrades,             true,      true,       false },
    { "getconfirmationkey",     &getconfirmationkey,     true,      true,       false },
    { "confirmfiatpayment",     &confirmfiatpayment,     true,      true,       false },

    /* Deposits */
    { "requestdepositaddress",  &requestdepositaddress,  true,      false,      false },
    { "requestprivateaddress",  &requestprivateaddress,  true,      false,      false },
    { "listdepositaddresses",   &listdepositaddresses,   true,      false,      false },
    { "transferprivateaddress", &transferprivateaddress, true,      false,      false },
    { "transferdeposit",        &transferdeposit,        true,      false,      false },
    { "withdrawdeposit",        &withdrawdeposit,        true,      false,      false },

    /* Mining */
    { "getmininginfo",          &getmininginfo,          true,      false,      false },
    { "getnetworkhashps",       &getnetworkhashps,       true,      false,      false },
    { "getdifficulty",          &getdifficulty,          true,      false,      false },
    
    /* Wallet */
    { "dumpprivkey",            &dumpprivkey,            true,      false,      true },
    { "getsalt",                &getsalt,                false,     false,      true },
    { "setsalt",                &setsalt,                false,     false,      true },
    { "getbalance",             &getbalance,             false,     false,      true },
    { "getnewaddress",          &getnewaddress,          true,      false,      true },
    { "listreceivedbyaddress",  &listreceivedbyaddress,  false,     false,      true },
    { "getunconfirmedbalance",  &getunconfirmedbalance,  false,     false,      true },
    { "importprivkey",          &importprivkey,          false,     false,      true },
    { "listunspent",            &listunspent,            false,     false,      true },
    { "sendtoaddress",          &sendtoaddress,          false,     false,      true },

    /* Wallet-enabled mining */
    { "getgenerate",            &getgenerate,            true,      false,      false },
    { "gethashespersec",        &gethashespersec,        true,      false,      false },
    { "setgenerate",            &setgenerate,            true,      true,       false },

};

CRPCTable::CRPCTable()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])); vcidx++)
    {
        const CRPCCommand *pcmd;

        pcmd = &vRPCCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
}

const CRPCCommand *CRPCTable::operator[](string name) const
{
    map<string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it == mapCommands.end())
        return NULL;
    return (*it).second;
}


bool HTTPAuthorized(map<string, string>& mapHeaders)
{
    string strAuth = mapHeaders["authorization"];
    if (strAuth.substr(0,6) != "Basic ")
        return false;
    string strUserPass64 = strAuth.substr(6); boost::trim(strUserPass64);
    string strUserPass = DecodeBase64(strUserPass64);
    return TimingResistantEqual(strUserPass, strRPCUserColonPass);
}

void ErrorReply(std::ostream& stream, const Object& objError, const Value& id)
{
    // Send error reply from json-rpc error object
    int nStatus = HTTP_INTERNAL_SERVER_ERROR;
    int code = find_value(objError, "code").get_int();
    if (code == RPC_INVALID_REQUEST) nStatus = HTTP_BAD_REQUEST;
    else if (code == RPC_METHOD_NOT_FOUND) nStatus = HTTP_NOT_FOUND;
    string strReply = JSONRPCReply(Value::null, objError, id);
    stream << HTTPReply(nStatus, strReply, false) << std::flush;
}

bool ClientAllowed(const boost::asio::ip::address& address)
{
    // Make sure that IPv4-compatible and IPv4-mapped IPv6 addresses are treated as IPv4 addresses
    if (address.is_v6()
     && (address.to_v6().is_v4_compatible()
      || address.to_v6().is_v4_mapped()))
        return ClientAllowed(address.to_v6().to_v4());

    if (address == asio::ip::address_v4::loopback()
     || address == asio::ip::address_v6::loopback()
     || (address.is_v4()
         // Check whether IPv4 addresses match 127.0.0.0/8 (loopback subnet)
      && (address.to_v4().to_ulong() & 0xff000000) == 0x7f000000))
        return true;

    const string strAddress = address.to_string();
    const vector<string>& vAllow = mapMultiArgs["-rpcallowip"];
    BOOST_FOREACH(string strAllow, vAllow)
        if (WildcardMatch(strAddress, strAllow))
            return true;
    return false;
}

class AcceptedConnection
{
public:
    virtual ~AcceptedConnection() {}

    virtual std::iostream& stream() = 0;
    virtual std::string peer_address_to_string() const = 0;
    virtual void close() = 0;
};

template <typename Protocol>
class AcceptedConnectionImpl : public AcceptedConnection
{
public:
    AcceptedConnectionImpl(
            asio::io_service& io_service,
            ssl::context &context,
            bool fUseSSL) :
        sslStream(io_service, context),
        _d(sslStream, fUseSSL),
        _stream(_d)
    {
    }

    virtual std::iostream& stream()
    {
        return _stream;
    }

    virtual std::string peer_address_to_string() const
    {
        return peer.address().to_string();
    }

    virtual void close()
    {
        _stream.close();
    }

    typename Protocol::endpoint peer;
    asio::ssl::stream<typename Protocol::socket> sslStream;

private:
    SSLIOStreamDevice<Protocol> _d;
    iostreams::stream< SSLIOStreamDevice<Protocol> > _stream;
};

void ServiceConnection(AcceptedConnection *conn);

// Forward declaration required for RPCListen
template <typename Protocol, typename SocketAcceptorService>
static void RPCAcceptHandler(boost::shared_ptr< basic_socket_acceptor<Protocol, SocketAcceptorService> > acceptor,
                             ssl::context& context,
                             bool fUseSSL,
                             boost::shared_ptr< AcceptedConnection > conn,
                             const boost::system::error_code& error);

/**
 * Sets up I/O resources to accept and handle a new connection.
 */
template <typename Protocol, typename SocketAcceptorService>
static void RPCListen(boost::shared_ptr< basic_socket_acceptor<Protocol, SocketAcceptorService> > acceptor,
                   ssl::context& context,
                   const bool fUseSSL)
{
    // Accept connection
    boost::shared_ptr< AcceptedConnectionImpl<Protocol> > conn(new AcceptedConnectionImpl<Protocol>(acceptor->get_io_service(), context, fUseSSL));

    acceptor->async_accept(
            conn->sslStream.lowest_layer(),
            conn->peer,
            boost::bind(&RPCAcceptHandler<Protocol, SocketAcceptorService>,
                acceptor,
                boost::ref(context),
                fUseSSL,
                conn,
                _1));
}


/**
 * Accept and handle incoming connection.
 */
template <typename Protocol, typename SocketAcceptorService>
static void RPCAcceptHandler(boost::shared_ptr< basic_socket_acceptor<Protocol, SocketAcceptorService> > acceptor,
                             ssl::context& context,
                             const bool fUseSSL,
                             boost::shared_ptr< AcceptedConnection > conn,
                             const boost::system::error_code& error)
{
    // Immediately start accepting new connections, except when we're cancelled or our socket is closed.
    if (error != asio::error::operation_aborted && acceptor->is_open())
        RPCListen(acceptor, context, fUseSSL);

    AcceptedConnectionImpl<ip::tcp>* tcp_conn = dynamic_cast< AcceptedConnectionImpl<ip::tcp>* >(conn.get());

    if (error)
    {
        // TODO: Actually handle errors
        LogPrintf("%s: Error: %s\n", __func__, error.message());
    }
    // Restrict callers by IP.  It is important to
    // do this before starting client thread, to filter out
    // certain DoS and misbehaving clients.
    else if (tcp_conn && !ClientAllowed(tcp_conn->peer.address()))
    {
        // Only send a 403 if we're not using SSL to prevent a DoS during the SSL handshake.
        if (!fUseSSL)
            conn->stream() << HTTPReply(HTTP_FORBIDDEN, "", false) << std::flush;
        conn->close();
    }
    else {
        ServiceConnection(conn.get());
        conn->close();
    }
}

void StartRPCThreads()
{
    strRPCUserColonPass = mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"];
    if (((mapArgs["-rpcpassword"] == "") ||
         (mapArgs["-rpcuser"] == mapArgs["-rpcpassword"])) && Params().RequireRPCPassword())
    {
        unsigned char rand_pwd[32];
        RAND_bytes(rand_pwd, 32);
        string strWhatAmI = "To use flexd";
        if (mapArgs.count("-server"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-server\"");
        else if (mapArgs.count("-daemon"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-daemon\"");
        uiInterface.ThreadSafeMessageBox(strprintf(
            _("%s, you must set a rpcpassword in the configuration file:\n"
              "%s\n"
              "It is recommended you use the following random password:\n"
              "rpcuser=flexrpc\n"
              "rpcpassword=%s\n"
              "(you do not need to remember this password)\n"
              "The username and password MUST NOT be the same.\n"
              "If the file does not exist, create it with owner-readable-only file permissions.\n"
              "It is also recommended to set alertnotify so you are notified of problems;\n"
              "for example: alertnotify=echo %%s | mail -s \"Flex Alert\" admin@foo.com\n"),
                strWhatAmI,
                GetConfigFile().string(),
                EncodeBase58(&rand_pwd[0],&rand_pwd[0]+32)),
                "", CClientUIInterface::MSG_ERROR);
        StartShutdown();
        return;
    }

    assert(rpc_io_service == NULL);
    rpc_io_service = new asio::io_service();
    rpc_ssl_context = new ssl::context(*rpc_io_service, ssl::context::sslv23);

    const bool fUseSSL = GetBoolArg("-rpcssl", false);

    if (fUseSSL)
    {
        rpc_ssl_context->set_options(ssl::context::no_sslv2);

        filesystem::path pathCertFile(GetArg("-rpcsslcertificatechainfile", "server.cert"));
        if (!pathCertFile.is_complete()) pathCertFile = filesystem::path(GetDataDir()) / pathCertFile;
        if (filesystem::exists(pathCertFile)) rpc_ssl_context->use_certificate_chain_file(pathCertFile.string());
        else LogPrintf("ThreadRPCServer ERROR: missing server certificate file %s\n", pathCertFile.string());

        filesystem::path pathPKFile(GetArg("-rpcsslprivatekeyfile", "server.pem"));
        if (!pathPKFile.is_complete()) pathPKFile = filesystem::path(GetDataDir()) / pathPKFile;
        if (filesystem::exists(pathPKFile)) rpc_ssl_context->use_private_key_file(pathPKFile.string(), ssl::context::pem);
        else LogPrintf("ThreadRPCServer ERROR: missing server private key file %s\n", pathPKFile.string());

        string strCiphers = GetArg("-rpcsslciphers", "TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH");
        SSL_CTX_set_cipher_list(rpc_ssl_context->impl(), strCiphers.c_str());
    }

    // Try a dual IPv6/IPv4 socket, falling back to separate IPv4 and IPv6 sockets
    const bool loopback = !mapArgs.count("-rpcallowip");
    asio::ip::address bindAddress = loopback ? asio::ip::address_v6::loopback() : asio::ip::address_v6::any();
    ip::tcp::endpoint endpoint(bindAddress, GetArg("-rpcport", Params().RPCPort()));
    boost::system::error_code v6_only_error;

    bool fListening = false;
    std::string strerr;
    try
    {
        boost::shared_ptr<ip::tcp::acceptor> acceptor(new ip::tcp::acceptor(*rpc_io_service));
        acceptor->open(endpoint.protocol());
        acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

        // Try making the socket dual IPv6/IPv4 (if listening on the "any" address)
        acceptor->set_option(boost::asio::ip::v6_only(loopback), v6_only_error);

        acceptor->bind(endpoint);
        acceptor->listen(socket_base::max_connections);

        RPCListen(acceptor, *rpc_ssl_context, fUseSSL);

        rpc_acceptors.push_back(acceptor);
        fListening = true;
    }
    catch(boost::system::system_error &e)
    {
        strerr = strprintf(_("An error occurred while setting up the RPC port %u for listening on IPv6, falling back to IPv4: %s"), endpoint.port(), e.what());
    }
    try {
        // If dual IPv6/IPv4 failed (or we're opening loopback interfaces only), open IPv4 separately
        if (!fListening || loopback || v6_only_error)
        {
            bindAddress = loopback ? asio::ip::address_v4::loopback() : asio::ip::address_v4::any();
            endpoint.address(bindAddress);

            boost::shared_ptr<ip::tcp::acceptor> acceptor(new ip::tcp::acceptor(*rpc_io_service));
            acceptor->open(endpoint.protocol());
            acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            acceptor->bind(endpoint);
            acceptor->listen(socket_base::max_connections);

            RPCListen(acceptor, *rpc_ssl_context, fUseSSL);

            rpc_acceptors.push_back(acceptor);
            fListening = true;
        }
    }
    catch(boost::system::system_error &e)
    {
        strerr = strprintf(_("An error occurred while setting up the RPC port %u for listening on IPv4: %s"), endpoint.port(), e.what());
    }

    if (!fListening) {
        uiInterface.ThreadSafeMessageBox(strerr, "", CClientUIInterface::MSG_ERROR);
        StartShutdown();
        return;
    }

    rpc_worker_group = new boost::thread_group();
    for (int i = 0; i < GetArg("-rpcthreads", 4); i++)
        rpc_worker_group->create_thread(boost::bind(&asio::io_service::run, rpc_io_service));
}

void StartDummyRPCThread()
{
    if(rpc_io_service == NULL)
    {
        rpc_io_service = new asio::io_service();
        /* Create dummy "work" to keep the thread from exiting when no timeouts active,
         * see http://www.boost.org/doc/libs/1_51_0/doc/html/boost_asio/reference/io_service.html#boost_asio.reference.io_service.stopping_the_io_service_from_running_out_of_work */
        rpc_dummy_work = new asio::io_service::work(*rpc_io_service);
        rpc_worker_group = new boost::thread_group();
        rpc_worker_group->create_thread(boost::bind(&asio::io_service::run, rpc_io_service));
    }
}

void StopRPCThreads()
{
    if (rpc_io_service == NULL) return;

    // First, cancel all timers and acceptors
    // This is not done automatically by ->stop(), and in some cases the destructor of
    // asio::io_service can hang if this is skipped.
    boost::system::error_code ec;
    BOOST_FOREACH(const boost::shared_ptr<ip::tcp::acceptor> &acceptor, rpc_acceptors)
    {
        acceptor->cancel(ec);
        if (ec)
            LogPrintf("%s: Warning: %s when cancelling acceptor", __func__, ec.message());
    }
    rpc_acceptors.clear();
    BOOST_FOREACH(const PAIRTYPE(std::string, boost::shared_ptr<deadline_timer>) &timer, deadlineTimers)
    {
        timer.second->cancel(ec);
        if (ec)
            LogPrintf("%s: Warning: %s when cancelling timer", __func__, ec.message());
    }
    deadlineTimers.clear();

    rpc_io_service->stop();
    if (rpc_worker_group != NULL)
        rpc_worker_group->join_all();
    delete rpc_dummy_work; rpc_dummy_work = NULL;
    delete rpc_worker_group; rpc_worker_group = NULL;
    delete rpc_ssl_context; rpc_ssl_context = NULL;
    delete rpc_io_service; rpc_io_service = NULL;
}

void RPCRunHandler(const boost::system::error_code& err, boost::function<void(void)> func)
{
    if (!err)
        func();
}

void RPCRunLater(const std::string& name, boost::function<void(void)> func, int64_t nSeconds)
{
    assert(rpc_io_service != NULL);

    if (deadlineTimers.count(name) == 0)
    {
        deadlineTimers.insert(make_pair(name,
                                        boost::shared_ptr<deadline_timer>(new deadline_timer(*rpc_io_service))));
    }
    deadlineTimers[name]->expires_from_now(posix_time::seconds(nSeconds));
    deadlineTimers[name]->async_wait(boost::bind(RPCRunHandler, _1, func));
}

class JSONRequest
{
public:
    Value id;
    string strMethod;
    Array params;

    JSONRequest() { id = Value::null; }
    void parse(const Value& valRequest);
};

void JSONRequest::parse(const Value& valRequest)
{
    // Parse request
    if (valRequest.type() != obj_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    const Object& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    Value valMethod = find_value(request, "method");
    if (valMethod.type() == null_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    if (valMethod.type() != str_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    strMethod = valMethod.get_str();
    if (strMethod != "getwork" && strMethod != "getblocktemplate")
        LogPrint("rpc", "ThreadRPCServer method=%s\n", strMethod);

    // Parse params
    Value valParams = find_value(request, "params");
    if (valParams.type() == array_type)
        params = valParams.get_array();
    else if (valParams.type() == null_type)
        params = Array();
    else
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
}


static Object JSONRPCExecOne(const Value& req)
{
    Object rpc_result;

    JSONRequest jreq;
    try {
        jreq.parse(req);

        Value result = tableRPC.execute(jreq.strMethod, jreq.params);
        rpc_result = JSONRPCReplyObj(result, Value::null, jreq.id);
    }
    catch (Object& objError)
    {
        rpc_result = JSONRPCReplyObj(Value::null, objError, jreq.id);
    }
    catch (std::exception& e)
    {
        rpc_result = JSONRPCReplyObj(Value::null,
                                     JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

static string JSONRPCExecBatch(const Array& vReq)
{
    Array ret;
    for (unsigned int reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
        ret.push_back(JSONRPCExecOne(vReq[reqIdx]));

    return write_string(Value(ret), false) + "\n";
}

void ServiceConnection(AcceptedConnection *conn)
{
    bool fRun = true;
    while (fRun && !ShutdownRequested())
    {
        int nProto = 0;
        map<string, string> mapHeaders;
        string strRequest, strMethod, strURI;

        // Read HTTP request line
        if (!ReadHTTPRequestLine(conn->stream(), nProto, strMethod, strURI))
            break;

        // Read HTTP message headers and body
        ReadHTTPMessage(conn->stream(), mapHeaders, strRequest, nProto);

        if (strURI != "/") {
            conn->stream() << HTTPReply(HTTP_NOT_FOUND, "", false) << std::flush;
            break;
        }

        // Check authorization
        if (mapHeaders.count("authorization") == 0)
        {
            conn->stream() << HTTPReply(HTTP_UNAUTHORIZED, "", false) << std::flush;
            break;
        }
        if (!HTTPAuthorized(mapHeaders))
        {
            LogPrintf("ThreadRPCServer incorrect password attempt from %s\n", conn->peer_address_to_string());
            /* Deter brute-forcing short passwords.
               If this results in a DoS the user really
               shouldn't have their RPC port exposed. */
            if (mapArgs["-rpcpassword"].size() < 20)
                MilliSleep(250);

            conn->stream() << HTTPReply(HTTP_UNAUTHORIZED, "", false) << std::flush;
            break;
        }
        if (mapHeaders["connection"] == "close")
            fRun = false;

        JSONRequest jreq;
        try
        {
            // Parse request
            Value valRequest;
            if (!read_string(strRequest, valRequest))
                throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");

            string strReply;

            // singleton request
            if (valRequest.type() == obj_type) {
                jreq.parse(valRequest);

                Value result = tableRPC.execute(jreq.strMethod, jreq.params);

                // Send reply
                strReply = JSONRPCReply(result, Value::null, jreq.id);

            // array of requests
            } else if (valRequest.type() == array_type)
                strReply = JSONRPCExecBatch(valRequest.get_array());
            else
                throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");

            conn->stream() << HTTPReply(HTTP_OK, strReply, fRun) << std::flush;
        }
        catch (Object& objError)
        {
            ErrorReply(conn->stream(), objError, jreq.id);
            break;
        }
        catch (std::exception& e)
        {
            ErrorReply(conn->stream(), JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
            break;
        }
    }
}

json_spirit::Value CRPCTable::execute(const std::string &strMethod, const json_spirit::Array &params) const
{
    // Find method
    const CRPCCommand *pcmd = tableRPC[strMethod];
    if (!pcmd)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");


    // Observe safe mode
    string strWarning = GetWarnings("rpc");
    if (strWarning != "" && !GetBoolArg("-disablesafemode", false) &&
        !pcmd->okSafeMode)
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);

    try
    {
        // Execute
        Value result;
        {
            if (pcmd->threadSafe)
                result = pcmd->actor(params, false);

            else {
                LOCK(cs_main);
                result = pcmd->actor(params, false);
            }
        }
        return result;
    }
    catch (std::exception& e)
    {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}

std::string HelpExampleCli(string methodname, string args){
    return "> flex-cli " + methodname + " " + args + "\n";
}

std::string HelpExampleRpc(string methodname, string args){
    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", "
        "\"method\": \"" + methodname + "\", \"params\": [" + args + "] }' -H 'content-type: text/plain;' http://127.0.0.1:8732/\n";
}

const CRPCTable tableRPC;
