#ifndef TELEPORT_THIRDPARTY
#define TELEPORT_THIRDPARTY


#include "trade/trade_messages.h"


/*************************
 *  TradeHandler
 */

    void TradeHandler::ValidateAndAcknowledgeTrade(
    	AcceptCommit accept_commit)
    {
        OrderCommit commit
            = tradedata[accept_commit.order_commit_hash]["commit"];
        AcceptOrder accept;
        accept = tradedata[commit.accept_order_hash]["accept_order"];
        Order order = tradedata[accept.order_hash]["order"];

        Currency currency = teleportnode.currencies[order.currency];
        vch_t payer_data, payee_data;

        if (order.side == ASK)
        {
            payer_data = order.auxiliary_data;
            payee_data = accept.auxiliary_data;
        }
        else
        {
            payee_data = order.auxiliary_data;
            payer_data = accept.auxiliary_data;
        }
        if (!currency.ValidateProposedFiatTransaction(payer_data,
                                                      payee_data,
                                                      order.size))
        {
            printf("Could not validate fiat transaction!\n");
            return;
        }
        uint160 accept_commit_hash = accept_commit.GetHash160();
        tradedata[accept_commit_hash]["ttp_validated"] = true;
        ThirdPartyTransactionAcknowledgement ack(accept_commit_hash);
        ack.Sign();
        uint160 ack_hash = ack.GetHash160();
        tradedata[accept_commit_hash]["acknowledgement"] = ack_hash;
        tradedata[ack_hash]["acknowledgement"] = ack;
        BroadcastMessage(ack);
    }


/*
 *  TradeHandler
 *************************/


#endif