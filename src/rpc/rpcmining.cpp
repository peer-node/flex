// Distributed under version 3 of the Gnu Affero GPL software license, see the accompanying
// file COPYING.

#include "crypto/point.h"
#include "rpc/rpcserver.h"
#include "base/chainparams.h"
#include "flexnode/init.h"
#include "net/net.h"
#include "flexnode/main.h"

#include <stdint.h>
#include "flexnode/wallet.h"
#include "flexnode/flexnode.h"

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace json_spirit;
using namespace std;

uint160 networkhashps160()
{
    uint160 credit_hash = flexnode.previous_mined_credit_hash;
    uint32_t batches_to_sample = 5;
    uint32_t i;
    uint160 total_difficulty = 0;
    for (i = 0; i < batches_to_sample; i++)
    {
        if (!creditdata[credit_hash].HasProperty("mined_credit"))
            break;
        MinedCredit credit = creditdata[credit_hash]["mined_credit"];
        total_difficulty += credit.difficulty;
    }
    if (i == 0)
        return 0;
    return (total_difficulty / (60 * i));
}

double estimatednumberofminers()
{
    uint160 credit_hash = flexnode.previous_mined_credit_hash;
    uint32_t batches_to_sample = 30;
    uint32_t i;
    uint160 total_difficulty = 0;
    uint64_t total_length = 0;

    for (i = 0; i < batches_to_sample; i++)
    {
        if (!creditdata[credit_hash].HasProperty("msg"))
            break;
        MinedCreditMessage msg = creditdata[credit_hash]["msg"];
        total_difficulty += msg.mined_credit.difficulty;
        total_length += msg.proof.Length();
        credit_hash = msg.mined_credit.previous_mined_credit_hash;
    }
    if (total_length == 0)
        return 0;
    return total_difficulty.getdouble() / total_length;
}

Value getnetworkhashps(const Array& params, bool fHelp)
{
    return (uint64_t)networkhashps160().getdouble();
}


Value getgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getgenerate\n"
            "\nReturn if the server is set to generate credits or not. The default is false.\n"
            "It is set with the command line argument -gen (or flex.conf setting gen)\n"
            "It can also be set with the setgenerate call.\n"
            "\nResult\n"
            "true|false      (boolean) If the server is set to generate credits or not\n"
            "\nExamples:\n"
            + HelpExampleCli("getgenerate", "")
            + HelpExampleRpc("getgenerate", "")
        );

    return (bool)flexnode.digging;
}


Value setgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setgenerate generate\n"
            "\nSet 'generate' true or false to turn generation on or off.\n"
            "\nArguments:\n"
            "1. generate         (boolean, required) Set to true to "
            "turn on generation, off to turn off.\n"
            "\nExamples:\n"
            "\nCheck the setting\n"
            + HelpExampleCli("getgenerate", "") +
            "\nTurn off generation\n"
            + HelpExampleCli("setgenerate", "false") +
            "\nUsing json rpc\n"
            + HelpExampleRpc("setgenerate", "true")
        );

    LOCK(flexnode.mutex);
    bool fGenerate = true;
    if (params.size() > 0)
        fGenerate = params[0].get_bool();

    if (fGenerate)
        flexnode.pit.StartMining();
    else
        flexnode.pit.StopMining();

    return Value::null;
}

Value gethashespersec(const Array& params, bool fHelp)
{
    return flexnode.pit.hashrate;
}

Value getmininginfo(const Array& params, bool fHelp)
{
    Object obj;
    obj.push_back(Pair("mining", (bool)flexnode.digging));
    obj.push_back(Pair("hashrate", flexnode.pit.hashrate));
    obj.push_back(Pair("networkhashrate",
                  (uint64_t)networkhashps160().getdouble()));
    obj.push_back(Pair("estimatednumberofminers", estimatednumberofminers()));
    return obj;
}
