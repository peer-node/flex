// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#include "crypto/point.h"
#include "base/base58.h"
#include "rpc/rpcserver.h"
#include "teleportnode/init.h"
#include "net/net.h"
#include "net/netbase.h"
#include "base/util.h"
#include "teleportnode/wallet.h"
#include "teleportnode/teleportnode.h"
#include "database/memdb.h"
#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

#include "log.h"
#define LOG_CATEGORY "rpcwallet.cpp"


using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;

int64_t nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;

Value getnewaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewaddress \n"
            "\nReturns a new Teleport address for receiving payments.\n"
            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
        );

    // Generate a new key that is added to wallet
    Point newKey = teleportnode.wallet.NewKey();
    if (!newKey)
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: failed!");
    CKeyID keyID(KeyHash(newKey));

    return CBitcoinAddress(keyID).ToString();
}

Value sendtoaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "sendtoaddress \"teleportaddress\" amount\n"
            "\nSent an amount to a given address. The amount is a real and is rounded to the nearest 0.00000001\n"
            "\nArguments:\n"
            "1. \"teleportaddress\"  (string, required) The address to send to.\n"
            "2. \"amount\"      (numeric, required) The amount in btc to send. eg 0.1\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1")
        );

    CBitcoinAddress address(params[0].get_str());

    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    CKeyID keyid;
    address.GetKeyID(keyid);
    vch_t keydata_(BEGIN(keyid), END(keyid));
    
    // Amount
    int64_t amount = AmountFromValue(params[1]);

    
    UnsignedTransaction rawtx = teleportnode.wallet.GetUnsignedTxGivenKeyData(
                                                                   keydata_,
                                                                   amount,
                                                                   false);
    if (!rawtx)
        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to generate unsigned tx");

    SignedTransaction tx = SignTransaction(rawtx);
    log_ << "signed transaction\nverifying\n";
    if (!VerifyTransactionSignature(tx))
        log_ << "signature verification failed!\n";
    else
        log_ << "signature verification succeeded!\n";
    teleportnode.HandleTransaction(tx, NULL);
    uint256 txhash = tx.GetHash();

    return ValueString(vch_t(BEGIN(txhash), END(txhash)));
}

Value getbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getbalance ( \"account\" minconf )\n"
            "\nIf account is not specified, returns the server's total available balance.\n"
            "If account is specified, returns the balance in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the balance in the default \"\" account.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, optional) The selected account, or \"*\" for entire wallet. It may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in btc received for this account.\n"
            "\nExamples:\n"
            "\nThe total amount in the server across all accounts\n"
            + HelpExampleCli("getbalance", "") +
            "\nThe total amount in the server across all accounts, with at least 5 confirmations\n"
            + HelpExampleCli("getbalance", "\"*\" 6") +
            "\nThe total amount in the default account with at least 1 confirmation\n"
            + HelpExampleCli("getbalance", "\"\"") +
            "\nThe total amount in the account named tabby with at least 6 confirmations\n"
            + HelpExampleCli("getbalance", "\"tabby\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getbalance", "\"tabby\", 6")
        );

    return  ValueFromAmount(teleportnode.wallet.Balance());
}

Value getunconfirmedbalance(const Array &params, bool fHelp)
{
    return 0;
}

Value ListReceived(const Array& params)
{
    Array ret;
    foreach_(const CreditInBatch& credit, teleportnode.wallet.credits)
    {
        Object obj;
        uint160 keyhash;

        if (credit.keydata.size() == 20)
        {
            keyhash = uint160(credit.keydata);
        }
        else
        {
            Point pubkey;
            pubkey.setvch(credit.keydata);
            keyhash = KeyHash(pubkey);
        }
        CKeyID address(keyhash);

        obj.push_back(Pair("address", CBitcoinAddress(address).ToString()));
        log_ << "ListReceived: keyhash for "
             << CBitcoinAddress(address).ToString()
             << " is " << keyhash << "\n";
        obj.push_back(Pair("amount", ValueFromAmount(credit.amount)));
        ret.push_back(obj);
    }

    return ret;
}

Value AggregateReceived(const Array& params)
{
    Array ret;
    vector<uint160> keyhashes;
    CMemoryDB totals;

    foreach_(const CreditInBatch& credit, teleportnode.wallet.credits)
    {
        Object obj;
        uint160 keyhash;

        if (credit.keydata.size() == 20)
        {
            keyhash = uint160(credit.keydata);
        }
        else
        {
            Point pubkey;
            pubkey.setvch(credit.keydata);
            keyhash = KeyHash(pubkey);
            for (uint32_t j = 0; j < credit.keydata.size(); j++)
                log_ << credit.keydata[j];
            log_ << "  " << credit.amount << "\n";
        }

        totals[keyhash] = (uint64_t)totals[keyhash] + credit.amount;
        if (!VectorContainsEntry(keyhashes, keyhash))
            keyhashes.push_back(keyhash);
    }
    foreach_(const uint160 keyhash, keyhashes)
    {
        Object obj;
        CKeyID address(keyhash);
        uint64_t amount = totals[keyhash];
        obj.push_back(Pair("address", CBitcoinAddress(address).ToString()));
        obj.push_back(Pair("amount", ValueFromAmount(amount)));
        ret.push_back(obj);
    }

    return ret;
}



Value listreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listreceivedbyaddress ( minconf )\n"
            "\nList balances by receiving address.\n"
            "\nArguments:\n"
            "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"


            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
            "    \"amount\" : x.xxx,                  (numeric) The total amount in btc received by the address\n"
            "    \"confirmations\" : n                (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaddress", "")
            + HelpExampleCli("listreceivedbyaddress", "6 true")
            + HelpExampleRpc("listreceivedbyaddress", "6, true")
        );

    return AggregateReceived(params);
}

Value listunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listunspent ( minconf )\n"
            "\nList unspent credits.\n"
            "\nArguments:\n"
            "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
            "    \"amount\" : x.xxx,                  (numeric) The total amount in btc received by the address\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaddress", "")
            + HelpExampleCli("listreceivedbyaddress", "6")
            + HelpExampleRpc("listreceivedbyaddress", "6")
        );

    return ListReceived(params);
}
