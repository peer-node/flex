#include "trade/trade.h"
#include "relays/relays.h"
#include "flexnode/flexnode.h"
#include "flexnode/schedule.h"
#include "relays/relaystate.h"
#include "credits/creditutil.h"

#include "log.h"
#define LOG_CATEGORY "trade.cpp"


using namespace std;

extern bool fRequestShutdown;


bool CheckPubKeyAuthorization(vch_t currency_code,
                              Signature pubkey_authorization,
                              Point order_pubkey,
                              Point fund_address_pubkey)
{
    Point fund_pubkey = fund_address_pubkey;

    log_ << "fund pubkey = " << fund_pubkey << " curve is "
         << fund_pubkey.curve << "\n";
    if (fund_pubkey.curve == SECP256K1)
    {
        return pubkey_authorization == Signature(0, 0) && 
                fund_pubkey == order_pubkey;
    }

    uint256 order_pubkey_hash = Hash(order_pubkey.getvch());
        
    return VerifySignatureOfHash(pubkey_authorization,
                                 order_pubkey_hash,
                                 fund_pubkey);
}

void SchedulePaymentCheck(uint160 payment_hash)
{
    flexnode.scheduler.Schedule("payment_check", payment_hash,
                                GetTimeMicros() + 9 * 1000 * 1000);
}

void DoScheduledPaymentCheck(uint160 payment_hash)
{
    CurrencyPaymentProof payment_proof
        = msgdata[payment_hash]["payment_proof"];
    HandleCurrencyPaymentProof(payment_proof, true);
}


void DoForwarding()
{
    CLocationIterator<> order_scanner;

    uint64_t started_handling, time_elapsed;
    int64_t time_to_sleep;
    uint160 hash;

    Priority priority, priority_;

    while (!fRequestShutdown)
    {
        order_scanner = tradedata.LocationIterator("forwarding");
        order_scanner.SeekEnd();

        while (order_scanner.GetPreviousObjectAndLocation(hash, priority))
        {
            priority_ = tradedata[hash]["forwarding"];

            started_handling = GetTimeMicros();
            string_t type = msgdata[hash]["type"];
            log_ << "DoForwarding(): found " << hash
                 << " with type " << type << "\n";

            tradedata[hash]["received"] = true;
            log_ << hash << " has been marked as received\n";
            if (!msgdata[hash]["forwarded"])
            {
                if (type == "order")
                {
                    Order order = msgdata[hash]["order"];
                    flexnode.tradehandler.BroadcastMessage(order);
                }
                else if (type == "accept_order")
                {
                    AcceptOrder accept = msgdata[hash]["accept_order"];
                    flexnode.tradehandler.BroadcastMessage(accept);
                }
            }

            msgdata[hash]["forwarded"] = true;
            tradedata.RemoveFromLocation("forwarding", priority);
            order_scanner = tradedata.LocationIterator("forwarding");
            order_scanner.SeekEnd();
            time_elapsed = GetTimeMicros() - started_handling;
            time_to_sleep = (1000000 / MAX_ORDER_RATE) - time_elapsed;
            boost::this_thread::interruption_point();
            if (time_to_sleep > 0)
                boost::this_thread::sleep(
                    boost::posix_time::milliseconds(time_to_sleep / 1000));
        }
        
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
}

void InitializeTradeTasks()
{
    flexnode.scheduler.AddTask(ScheduledTask("trade_check", 
                                    DoScheduledTradeInitiation));
    flexnode.scheduler.AddTask(ScheduledTask("payment_check", 
                                    DoScheduledPaymentCheck));
    flexnode.scheduler.AddTask(ScheduledTask("cancellation", 
                                    DoScheduledCancellation));
    flexnode.scheduler.AddTask(ScheduledTask("timeout", 
                                    DoScheduledTimeout));
    flexnode.scheduler.AddTask(ScheduledTask("secrets_check", 
                                    DoScheduledSecretsCheck));
    flexnode.scheduler.AddTask(ScheduledTask("secrets_release_check", 
                                    DoScheduledSecretsReleaseCheck));
    flexnode.scheduler.AddTask(ScheduledTask("send_backup_secrets", 
                                    DoScheduledBackupSecretsReleaseCheck));
    flexnode.scheduler.AddTask(ScheduledTask("secrets_released_check", 
                                    DoScheduledSecretsReleasedCheck));
    flexnode.scheduler.AddTask(ScheduledTask("credit_payment_check", 
                                    DoScheduledCreditPaymentCheck));
    boost::thread t(&DoForwarding);
}

void HandleCurrencyPaymentProof(CurrencyPaymentProof payment_proof,
                                bool scheduled)
{
    log_ <<"HandleCurrencyPaymentProof: " << payment_proof;
    if (!scheduled && !CheckCurrencyPaymentProof(payment_proof))
    {
        log_ << "failed check!\n";
        return;
    }
    uint160 payment_hash = payment_proof.GetHash160();
    uint160 accept_commit_hash = payment_proof.accept_commit_hash;

    tradedata[accept_commit_hash]["proof"] = payment_hash;
    if (tradedata[accept_commit_hash]["my_buy"])
    {
        log_ << "mine - confirming\n";
        AcceptCommit accept_commit 
            = msgdata[accept_commit_hash]["accept_commit"];
        OrderCommit order_commit
            = msgdata[accept_commit.order_commit_hash]["commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];

        Currency currency = flexnode.currencies[order.currency];
        string proof(payment_proof.proof.begin(), payment_proof.proof.end());

        bool check_result = false;

        Point escrow_key = GetEscrowCurrencyPubKey(order_commit, accept_commit);
        
        check_result = currency.crypto.VerifyPayment(proof, escrow_key, 
                                                     order.size, 1);
        
        if (!check_result)
        {
            log_ << "no funds. scheduling check\n";
            SchedulePaymentCheck(payment_hash);
            return;
        }
        log_ << "sending confirmation\n";
        SendCurrencyPaymentConfirmation(accept_commit_hash,
                                        payment_hash);
        log_ << "releasing funds\n";
        ReleaseCreditToCounterParty(accept_commit_hash);
    }
    else if (tradedata[accept_commit_hash]["ttp_validated"])
    {
        DoThirdPartyConfirmationCheck(payment_proof);
    }
    else
    {
        log_ << "I'm neither the buyer nor the ttp for this trade\n";
    }
}

void DoThirdPartyConfirmationCheck(CurrencyPaymentProof payment_proof)
{
    uint160 accept_commit_hash = payment_proof.accept_commit_hash;

    AcceptCommit accept_commit 
        = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit order_commit
        = msgdata[accept_commit.order_commit_hash]["commit"];
    AcceptOrder accept
        = msgdata[order_commit.accept_order_hash]["accept_order"];
    Order order = msgdata[accept_commit.order_hash]["order"];
    
    vch_t payer_data = (order.side == ASK)
                       ? order.auxiliary_data : accept.auxiliary_data;

    vch_t payee_data = (order.side == BID)
                       ? order.auxiliary_data : accept.auxiliary_data;

    Currency currency = flexnode.currencies[order.currency];
    if (currency.VerifyFiatPayment(payment_proof.proof, 
                                   payer_data, 
                                   payee_data, 
                                   order.size))
    {
        uint160 acknowledgement_hash
            = tradedata[accept_commit_hash]["acknowledgement"];

        ThirdPartyPaymentConfirmation confirmation(acknowledgement_hash);
        flexnode.tradehandler.BroadcastMessage(confirmation);
    }
}


bool CheckForCreditPayment(uint160 accept_commit_hash)
{
    uint64_t credit_received = tradedata[accept_commit_hash]["received"];
    uint64_t amount_due = tradedata[accept_commit_hash]["due"];
 
    bool credit_paid = credit_received >= amount_due;

    if (credit_paid)
    {
        uint64_t credit_payment_time
            = tradedata[accept_commit_hash]["credit_pay_time"];
        if (!credit_payment_time)
            tradedata[accept_commit_hash]["credit_pay_time"] = GetTimeMicros();
    }
    return credit_paid;
}


void RecordConfirmation(uint160 accept_commit_hash)
{
    uint64_t confirmation_time 
        = tradedata[accept_commit_hash]["confirmation_time"];
    if (!confirmation_time)
        tradedata[accept_commit_hash]["confirmation_time"] = GetTimeMicros();
    tradedata[accept_commit_hash]["currency_paid"] = true;
    log_ << "currency paid!\n";
}

void HandleCurrencyPaymentConfirmation(CurrencyPaymentConfirmation confirmation)
{
    log_ << "HandlePaymentConfirmation()\n";
    uint160 accept_commit_hash = confirmation.accept_commit_hash;

    if (!CheckPaymentConfirmation(confirmation))
    {
        log_ << "payment confirmation check failed\n";
        return;
    }
    
    RecordConfirmation(accept_commit_hash);
    tradedata[accept_commit_hash]["payment_confirmation_received"] = true;

    if (tradedata[accept_commit_hash]["my_sell"])
    {
        log_ << "my sell - scheduling secrets check\n";
        ScheduleSecretsCheck(accept_commit_hash, 15);
    }

    flexnode.scheduler.Schedule("secrets_released_check", 
                                accept_commit_hash, 
                                GetTimeMicros() + DO_SUCCESSION_AFTER);
}



void RecordComplaintAboutSecret(uint160 accept_commit_hash, uint64_t position)
{
    vector<uint64_t> positions = tradedata[accept_commit_hash]["complaints_about_secrets"];
    if (!VectorContainsEntry(positions, position))
        positions.push_back(position);
    tradedata[accept_commit_hash]["complaints_about_secrets"] = positions;
}


void WatchForCreditPayment(OrderCommit order_commit, 
                           AcceptCommit accept_commit)
{
    Point escrow_key = GetEscrowCreditPubKey(order_commit, accept_commit);
    Order order = msgdata[order_commit.order_hash]["order"];
    uint64_t amount = order.size * order.integer_price;
    uint160 accept_commit_hash = accept_commit.GetHash160();
    tradedata[escrow_key]["accept_commit_hash"] = accept_commit_hash;
    log_ << "escrow key " << escrow_key << " has accept commit hash "
         << accept_commit_hash << "\n";
    tradedata[accept_commit_hash]["due"] = amount;
    log_ << amount << " is due for accept_commit_hash "
         << accept_commit_hash << "\n";
}

void WatchForCreditPayment(uint160 accept_commit_hash)
{
    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    return WatchForCreditPayment(commit, accept_commit);
}


/*************************
 *  TradeHandler
 */


    void TradeHandler::HandleOrder(Order order)
    {
        log_ << "handling:" << order;
        if (!ValidateOrder(order))
        {
            log_ << "couldn't validate order!\n";
            should_forward = false;
            return;
        }
        uint160 order_hash = order.GetHash160();
        marketdata[order_hash]["order"] = order;
        PriceAndTime price_and_time(GetTimeMicros(), order.integer_price);
        BookSide book_side(order.side, order.currency);
        marketdata[order_hash].Location(book_side) = price_and_time;

        ScheduleForwarding(order, order_hash);

        flexnode.event_notifier.RecordEvent(order_hash);
    }


    void TradeHandler::HandleAcceptCommit(AcceptCommit accept_commit)
    {
        log_ << "handling accept_commit: " << accept_commit;
        uint64_t now = GetTimeMicros();
        if (!ValidateAcceptCommit(accept_commit))
        {
            should_forward = false;
            return;
        }
        Order order = msgdata[accept_commit.order_hash]["order"];
        
        log_ << "validated\n"
             << "order commit hash was "
             << accept_commit.order_commit_hash << "\n";

        tradedata[accept_commit.order_hash]["accept_commit"] = accept_commit;
        uint160 accept_commit_hash = accept_commit.GetHash160();
        
        flexnode.scheduler.Schedule("timeout", accept_commit_hash, 
                                    GetTimeMicros()
                                      + order.timeout * 1000 * 1000);
        if (!CheckForMyRelaySecrets(accept_commit))
            return;

        if (tradedata[accept_commit_hash]["i_have_secret"])
        {
            flexnode.scheduler.Schedule("secrets_release_check", 
                                        accept_commit_hash, 
                                        GetTimeMicros() 
                                            + COMPLAINT_WAIT_TIME);
            
        }

        if (tradedata[accept_commit_hash]["i_have_secret"] ||
            tradedata[accept_commit.order_commit_hash]["is_mine"] ||
            tradedata[accept_commit_hash]["is_mine"])
        {
            log_ << "watching for credit payment for " 
                 << accept_commit_hash << "\n";
            WatchForCreditPayment(accept_commit_hash);
        }

        if (tradedata[accept_commit.order_commit_hash]["is_mine"])
        {
            log_ << "my order - scheduling initiation of trade\n";
            ScheduleTradeInitiation(accept_commit_hash);
            SendDisclosure(accept_commit_hash, accept_commit, order, false);
            if (order.side == BID)
            {
                log_ << "my buy\n";
                tradedata[accept_commit_hash]["my_buy"] = true;
            }
            else
            {
                log_ << "my sell\n";
                tradedata[accept_commit_hash]["my_sell"] = true;
            }
            uint160 order_hash = accept_commit.order_hash;
            tradedata[order_hash]["accept_commit_hash"] = accept_commit_hash;
            tradedata[accept_commit_hash].Location("my_trades") = now;
        }
        if (tradedata[accept_commit_hash]["is_mine"])
        {
            log_ << "my accept order\n";
            if (order.side == BID)
            {
                log_ << "my sell\n";
                tradedata[accept_commit_hash]["my_sell"] = true;
            }
            else
            {
                log_ << "my buy\n";
                tradedata[accept_commit_hash]["my_buy"] = true;
            }
            tradedata[accept_commit_hash].Location("my_trades") = now;
        }

        if (tradedata[accept_commit_hash]["my_sell"])
            WatchEscrowCreditPubKey(accept_commit_hash);

        marketdata.Delete(accept_commit.order_hash);
        flexnode.event_notifier.RecordEvent(accept_commit_hash);
        
        if (!order.is_cryptocurrency)
        {
            tradedata[accept_commit_hash]["fiat_trade"] = true;
            if (keydata[order.confirmation_key].HasProperty("privkey") &&
                CurrencyInfo(order.currency, "actasthirdparty") != "")
            {
                ValidateAndAcknowledgeTrade(accept_commit);
            }
        }
    }

/*
 *  TradeHandler
 *************************/
