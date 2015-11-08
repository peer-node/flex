#ifndef FLEX_THIRDPARTY
#define FLEX_THIRDPARTY

#include "flexnode/flexnode.h"

#include "log.h"
#define LOG_CATEGORY "thirdparty.cpp"


/*************************
 *  TradeHandler
 */

    void TradeHandler::ValidateAndAcknowledgeTrade(
    	AcceptCommit accept_commit)
    {
        uint160 accept_commit_hash = accept_commit.GetHash160();
    	uint160 commit_hash = accept_commit.order_commit_hash;
        OrderCommit commit = msgdata[commit_hash]["commit"];
        AcceptOrder accept;
        accept = msgdata[commit.accept_order_hash]["accept_order"];
        Order order = msgdata[accept.order_hash]["order"];

        Currency currency = flexnode.currencies[order.currency];
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
        if (!currency.ValidateProposedFiatTransaction(accept_commit_hash,
                                                      payer_data, 
                                                      payee_data, 
                                                      order.size))
        {
            log_ << "Could not validate fiat transaction!\n";
            return;
        }
        
        tradedata[accept_commit_hash]["ttp_validated"] = true;
        ThirdPartyTransactionAcknowledgement ack(accept_commit_hash);
        ack.Sign();
        uint160 ack_hash = ack.GetHash160();
        tradedata[accept_commit_hash]["acknowledgement"] = ack_hash;
        BroadcastMessage(ack);
    }


/*
 *  TradeHandler
 *************************/


#endif