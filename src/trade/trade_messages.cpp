#include "teleportnode/teleportnode.h"

#include "log.h"
#define LOG_CATEGORY "trade_messages.cpp"


/************
 *  Order
 */


    Order::Order(vch_t currency, uint8_t side,
          uint64_t size,   uint64_t integer_price,
          string_t auxiliary_data):
        currency(currency),
        side(side),
        size(size),
        integer_price(integer_price),
        auxiliary_data(auxiliary_data.begin(), auxiliary_data.end())
    {
        uint160 relay_chooser = Rand160();
        relay_chooser_hash = HashUint160(relay_chooser);
        tradedata[relay_chooser_hash]["data"] = relay_chooser;
        timestamp = GetTimeMicros();
        timeout = 1800;
        successfully_funded = GetFundAddressPubKeyAndProof();
        is_cryptocurrency = teleportnode.currencies[currency].is_cryptocurrency;
        log_ << "Order(): " << currency << "  crypto: "
             << is_cryptocurrency << "\n";

        if (!is_cryptocurrency)
        {

            string_t conf_key = CurrencyInfo(currency, "confirmationkey");
            log_ << "adding confirmationkey " << conf_key << "\n";
            vch_t conf_key_bytes;
            if (!DecodeBase58Check(conf_key.c_str(), conf_key_bytes))
            {
                return;
                size = 0;
            }
            confirmation_key.setvch(conf_key_bytes);
        }
        else
            confirmation_key = Point(SECP256K1, 0);
    }

    bool Order::GetFundAddressPubKeyAndProof()
    {
        log_ << "GetFundAddressPubKeyAndProof()\n";
        if (side == BID)
            return GetCreditPubKeyAndProof();

        if (teleportnode.currencies[currency].is_cryptocurrency)
            return GetCryptoCurrencyPubKeyAndProof();

        return GetFiatPubKeyAndProof();
    }

/*
 *  Order
 ***********/

string OrderStatus(uint160 order_hash);

/************************
 *  TraderLeaveMessage
 */

    TraderLeaveMessage::TraderLeaveMessage()
    {
        uint64_t start_time = initdata["instance"]["start_time"];
        CLocationIterator<> order_scanner;
        order_scanner = tradedata.LocationIterator("my_orders");
        uint160 order_hash, accept_commit_hash;
        uint64_t order_time;

        order_scanner.Seek(start_time);
        while (order_scanner.GetNextObjectAndLocation(order_hash, order_time))
        {
            if (OrderStatus(order_hash) == "active")
                order_hashes.push_back(order_hash);
        }
    }

/*
 *  TraderLeaveMessage
 ************************/



/*****************
 *  AcceptOrder
 */

    AcceptOrder::AcceptOrder(Order order, string_t auxiliary_data):
        auxiliary_data(auxiliary_data.begin(), auxiliary_data.end())
    {
        order_hash = order.GetHash160();
        currency = order.currency;

        accept_side = (order.side == BID) ? ASK : BID;
        amount = (order.side == BID)
                 ? order.size
                 : order.size * order.integer_price;

        relay_chooser = Rand160();

        uint160 latest_hash = teleportnode.previous_mined_credit_hash;

        MinedCredit latest_credit = creditdata[latest_hash]["mined_credit"];

        previous_credit_hash = latest_credit.previous_mined_credit_hash;
        log_ << "AcceptOrder(): previous_credit_hash="
             << previous_credit_hash << "\n";

        successfully_funded = GetFundAddressPubKeyAndProof();
    }

    bool AcceptOrder::GetFundAddressPubKeyAndProof()
    {
        log_ << "GetFundAddressPubKeyAndProof()\n";
        if (accept_side == BID)
            return GetCreditPubKeyAndProof();

        if (teleportnode.currencies[currency].is_cryptocurrency)
            return GetCryptoCurrencyPubKeyAndProof();

        return GetFiatPubKeyAndProof();
    }

/*
 *  AcceptOrder
 *****************/


/*****************
 *  OrderCommit
 */

    OrderCommit::OrderCommit(AcceptOrder accept_order): 
        order_hash(accept_order.order_hash),
        accept_order_hash(accept_order.GetHash160())
    {
        Order order = msgdata[order_hash]["order"];
        log_ << "retrieved order\n";
        Currency currency = teleportnode.currencies[order.currency];
        if (currency.is_cryptocurrency)
            curve = currency.crypto.curve;
        else
            curve = SECP256K1;
        relay_chooser = tradedata[order.relay_chooser_hash]["data"];
        log_ << "got relay chooser; generating secrets\n";
        distributed_trade_secret
            = DistributedTradeSecret(curve,
                                     accept_order.previous_credit_hash,
                                     relay_chooser 
                                        ^ accept_order.relay_chooser);

        uint160 second_relay_chooser = Rand160();
        second_relay_chooser_hash = HashUint160(second_relay_chooser);
        tradedata[second_relay_chooser_hash]["data"] = second_relay_chooser;

        log_ << "encrypting secrets\n";
        GenerateSecrets(accept_order);
        EncryptSecrets(accept_order);
    }

/*
 *  OrderCommit
 *****************/
