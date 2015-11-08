#include <boost/algorithm/string.hpp>
#include "boost/assign.hpp"
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "flexnode/eventnotifier.h"
#include "rpc/rpcprotocol.h"
#include "rpc/rpcobjects.h"
#include "flexnode/flexnode.h"


#include "log.h"
#define LOG_CATEGORY "eventnotifier.cpp"

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace json_spirit;


bool SendNotifications(const Array& notifications)
{
    log_ << "SendNotifications()\n";
    asio::io_service io_service;
    ssl::context context(io_service, ssl::context::sslv23);
    context.set_options(ssl::context::no_sslv2);
    asio::ssl::stream<asio::ip::tcp::socket> sslStream(io_service, context);
    SSLIOStreamDevice<asio::ip::tcp> d(sslStream, false);
    iostreams::stream< SSLIOStreamDevice<asio::ip::tcp> > stream(d);

    string notification_string = write_string(Value(notifications), false) 
                                              + "\n";
 
    log_ << "host is " << GetArg("-guihost", "127.0.0.1")
         << " and port is " << GetArg("-guiport", "5000") << "\n";
    bool fConnected = d.connect(GetArg("-guihost", "127.0.0.1"), 
                                GetArg("-guiport", "5000"));
    if (!fConnected)
        return false;

    StringMap headers;

    // Send request
    
    string post = HTTPPost(notification_string, headers);
    stream << post << std::flush;

    // Receive HTTP reply status
    int protocol = 0;
    int status = ReadHTTPStatus(stream, protocol);

    // Receive HTTP reply message headers and body
    StringMap response_headers;
    string reply;
    ReadHTTPMessage(stream, response_headers, reply, protocol);

    log_ << "status is " << status << "  reply is " << reply << "\n";

    if (status != 200)
        return false;

    return true;
}

void NotifyGuiOfBalance(vch_t currency, double balance)
{
    uint160 balance_hash = GetTimeMicros();

    guidata[balance_hash]["currency"] = string(currency.begin(),
                                               currency.end());
    guidata[balance_hash]["balance"] = balance;
    guidata[balance_hash].Location("events") = GetTimeMicros();
}

Object GetBalance(uint160 balance_hash)
{
    string currency = guidata[balance_hash]["currency"];
    double balance = guidata[balance_hash]["balance"];
    Object balance_event;
    balance_event.push_back(Pair("currency", currency));
    balance_event.push_back(Pair("balance", balance));
    return balance_event;
}


Object GetEventFromOrder(uint160 order_hash)
{
    return GetObjectFromOrder(msgdata[order_hash]["order"]);
}

Object GetEventFromCancelOrder(uint160 cancel_hash)
{
    CancelOrder cancel = msgdata[cancel_hash]["cancel"];
    Object cancel_event;
    cancel_event.push_back(Pair("order_hash", cancel.order_hash.ToString()));
    return cancel_event;
}

Object GetEventFromAcceptOrder(uint160 accept_hash)
{
    AcceptOrder accept = msgdata[accept_hash]["accept"];
    Object accept_event;
    accept_event.push_back(Pair("order_hash", accept.order_hash.ToString()));
    return accept_event;
}

Object GetEventFromOrderCommit(uint160 commit_hash)
{
    OrderCommit commit = msgdata[commit_hash]["commit"];
    Object commit_event;
    commit_event.push_back(Pair("order_hash", commit.order_hash.ToString()));
    return commit_event;
}

Object GetEventFromAcceptCommit(uint160 accept_commit_hash)
{
    return GetObjectFromTrade(accept_commit_hash);
}

Object GetEventFromPaymentConfirmation(uint160 confirmation_hash)
{
    CurrencyPaymentConfirmation confirmation;
    confirmation = msgdata[confirmation_hash]["confirmation"];
    Object confirmation_event;
    confirmation_event.push_back(Pair("order_hash",
                                      confirmation.order_hash.ToString()));
    return confirmation_event;
}

Object GetEventFromThirdPartyConfirmation(uint160 confirmation_hash)
{
    ThirdPartyPaymentConfirmation confirmation;
    confirmation = msgdata[confirmation_hash]["confirmation"];
    Object confirmation_event;
    confirmation_event.push_back(Pair("order_hash",
                                      confirmation.order_hash.ToString()));
    return confirmation_event;
}

Object GetPaymentPrompt(uint160 prompt_hash)
{
    string payee = guidata[prompt_hash]["payee"];
    string payer = guidata[prompt_hash]["payer"];
    string url = guidata[prompt_hash]["payment_url"];
    double amount = guidata[prompt_hash]["amount"];

    Object prompt;
    prompt.push_back(Pair("payment_url", url));
    prompt.push_back(Pair("payee", payee));
    prompt.push_back(Pair("payer", payer));
    prompt.push_back(Pair("amount", amount));
    return prompt;
}

Object GetNotificationFromEvent(Object event, string type)
{
    Object notification;
    notification.push_back(Pair("type", type));
    notification.push_back(Pair(type, event));
    return notification;
}

Object GetEventFromMinedCredit(uint160 credit_hash)
{
    return GetObjectFromCredit(creditdata[credit_hash]["mined_credit"]);
}

Object GetNotificationFromEventHash(uint160 event_hash)
{
    string type = msgdata[event_hash]["type"];
    Object event;

    if (type == "order")
        event = GetEventFromOrder(event_hash);
    else if (type == "cancel")
        event = GetEventFromCancelOrder(event_hash);
    else if (type == "accept")
        event = GetEventFromAcceptOrder(event_hash);
    else if (type == "commit")
        event = GetEventFromOrderCommit(event_hash);
    else if (type == "accept_commit")
        event = GetEventFromAcceptCommit(event_hash);
    else if (type == "confirmation")
        event = GetEventFromPaymentConfirmation(event_hash);
    else if (type == "third_party_confirmation")
        event = GetEventFromThirdPartyConfirmation(event_hash);
    else if (type == "mined_credit")
        event = GetEventFromMinedCredit(event_hash);

    else if (guidata[event_hash].HasProperty("balance"))
    {
        event = GetBalance(event_hash);
        type = "balance";
    }
    else if (guidata[event_hash].HasProperty("payment_url"))
    {
        event = GetPaymentPrompt(event_hash);;
        type = "payment_prompt";
    }
    return GetNotificationFromEvent(event, type);
}
