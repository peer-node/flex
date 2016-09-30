// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef TELEPORT_TRADE
#define TELEPORT_TRADE

#include "trade/trader.h"
#include "trade/relay.h"
#include "trade/market.h"
#include "net/resourcemonitor.h"

#define SEND_RELAY_SECRETS_AFTER (25 * 1000 * 1000)     // 25 seconds
#define SEND_BACKUP_SECRETS_AFTER (50 * 1000 * 1000)    // 50 seconds
#define DO_SUCCESSION_AFTER (300 * 1000 * 1000)          // 300 seconds

#define MAX_ORDER_RATE 50 // per second

#define CHOOSE_NEW_RELAYS_AFTER (5 * 60 * 1000000)

#include "log.h"
#define LOG_CATEGORY "trade.h"


void SchedulePaymentCheck(uint160 payment_hash);

void DoScheduledPaymentCheck(uint160 payment_hash);


void InitializeTradeTasks();

vch_t ProofFromBranch(vector<uint160> branch);

uint160 PlaceOrder(vch_t currency, uint8_t side, 
                   uint64_t size, uint64_t integer_price,
                   uint64_t difficulty,
                   string_t auxiliary_data);

uint160 SendAccept(Order order, uint64_t difficulty, string_t auxiliary_data);


template <typename MSG>
bool CheckWork(MSG order_or_accept)
{
    if (order_or_accept.proof_of_work.link_threshold !=
        order_or_accept.proof_of_work.target << 6)
        return false;

    if (order_or_accept.proof_of_work.N_factor != 15 || 
        order_or_accept.proof_of_work.num_segments != 32)
        return false;

    MSG masked_order_or_accept = order_or_accept;

    masked_order_or_accept.signature = Signature(0, 0);
    masked_order_or_accept.proof_of_work = TwistWorkProof();

    uint256 prework_hash = masked_order_or_accept.GetHash();
    if (order_or_accept.proof_of_work.memoryseed != prework_hash)
        return false;
    if (!order_or_accept.proof_of_work.WorkDone() || 
        !order_or_accept.proof_of_work.SpotCheck().valid)
        return false;
    return true;
}

bool CheckForCreditPayment(uint160 accept_commit_hash);

void ScheduleCreditPaymentCheck(uint160 accept_commit_hash,
                                uint32_t seconds);

void DoScheduledCreditPaymentCheck(uint160 accept_commit_hash);


bool CheckPaymentConfirmation(CurrencyPaymentConfirmation confirmation);

void HandleCurrencyPaymentConfirmation(CurrencyPaymentConfirmation confirmation);

template <typename MSG> void HandleSecretMessage(MSG message);

template <typename MSG> void HandleSecret(MSG message, uint8_t direction);

template <typename MSG> void HandleSecret(MSG message, 
                                         uint8_t side, 
                                         uint8_t direction);

void WatchForCreditPayment(uint160 accept_commit_hash);
void WatchEscrowCreditPubKey(uint160 accept_commit_hash);

void DoThirdPartyConfirmationCheck(CurrencyPaymentProof payment_proof);

class TradeHandler : public MessageHandlerWithOrphanage
{
public:
    const char *channel;

    TradeHandler()
    {
        channel = "trade";
    }

    template <typename T>
    void BroadcastMessage(T message)
    {
        CDataStream ss = GetTradeBroadcastStream(message);
        uint160 message_hash = message.GetHash160();
        RelayTradeMessage(ss);
        if (!tradedata[message_hash]["received"])
        {
            log_ << message_hash << " has NOT been received - handling\n";
            HandleMessage(ss, NULL);
        }
            
    }

    template <typename T>
    CDataStream GetTradeBroadcastStream(T message)
    {
        if (message.signature == Signature(0, 0))
            message.Sign();
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss.reserve(10000);
        
        string string_message_genus(channel);
        ss << string_message_genus;
        
        string string_message_species = message.Type();
        ss << string_message_species;
        
        ss << message;
        return ss;
    }
    
    HandleClass_(Order);
    HandleClass_(CancelOrder);
    HandleClass_(AcceptOrder);
    HandleClass_(OrderCommit);
    HandleClass_(AcceptCommit);
    HandleClass_(SecretDisclosureMessage);
    
    HandleClass_(TradeSecretMessage);
    HandleClass_(CounterpartySecretMessage);
    
    HandleClass_(CurrencyPaymentProof);
    HandleClass_(CurrencyPaymentConfirmation);
    HandleClass_(ThirdPartyTransactionAcknowledgement);
    HandleClass_(ThirdPartyPaymentConfirmation);
    
    HandleClass_(RelayComplaint);
    HandleClass_(TraderComplaint);
    HandleClass_(RefutationOfTraderComplaint);
    HandleClass_(RefutationOfRelayComplaint);
    
    HandleClass_(NewRelayChoiceMessage);
    HandleClass_(TraderLeaveMessage);

    void HandleMessage(CDataStream ss, CNode *peer)
    {
        string message_type;
        ss >> message_type;
        ss >> message_type;
        uint160 message_hash = Hash160(ss.begin(), ss.end());
        tradedata[message_hash]["received"] = true;
        log_ << "handling message with hash " 
             << message_hash << " and type " << message_type << "\n";
        
        should_forward = true;
        handle_stream_(Order, ss, peer);
        handle_stream_(CancelOrder, ss, peer);
        handle_stream_(AcceptOrder, ss, peer);
        handle_stream_(OrderCommit, ss, peer);
        handle_stream_(AcceptCommit, ss, peer);
        handle_stream_(SecretDisclosureMessage, ss, peer);
        handle_stream_(TradeSecretMessage, ss, peer);
        handle_stream_(CurrencyPaymentProof, ss, peer);
        handle_stream_(CurrencyPaymentConfirmation, ss, peer);
        handle_stream_(CounterpartySecretMessage, ss, peer);
        handle_stream_(ThirdPartyTransactionAcknowledgement, ss, peer);
        handle_stream_(ThirdPartyPaymentConfirmation, ss, peer);
        handle_stream_(RelayComplaint, ss, peer);
        handle_stream_(TraderComplaint, ss, peer);
        handle_stream_(RefutationOfRelayComplaint, ss, peer);
        handle_stream_(RefutationOfTraderComplaint, ss, peer);
        handle_stream_(NewRelayChoiceMessage, ss, peer);
        handle_stream_(TraderLeaveMessage, ss, peer);
    }

    void HandleMessage(uint160 message_hash)
    {
        string_t message_type = msgdata[message_hash]["type"];
        log_ << "handling message with hash " << message_hash << "\n";
            
        handle_hash_(Order, message_hash);
        handle_hash_(CancelOrder, message_hash);
        handle_hash_(AcceptOrder, message_hash);
        handle_hash_(OrderCommit, message_hash);
        handle_hash_(AcceptCommit, message_hash);
        handle_hash_(SecretDisclosureMessage, message_hash);
        handle_hash_(TradeSecretMessage, message_hash);
        handle_hash_(CurrencyPaymentProof, message_hash);
        handle_hash_(CurrencyPaymentConfirmation, message_hash);
        handle_hash_(CounterpartySecretMessage, message_hash);
        handle_hash_(ThirdPartyTransactionAcknowledgement, message_hash);
        handle_hash_(ThirdPartyPaymentConfirmation, message_hash);
        handle_hash_(RelayComplaint, message_hash);
        handle_hash_(TraderComplaint, message_hash);
        handle_hash_(RefutationOfRelayComplaint, message_hash);
        handle_hash_(RefutationOfTraderComplaint, message_hash);
        handle_hash_(NewRelayChoiceMessage, message_hash);
        handle_hash_(TraderLeaveMessage, message_hash);
    }

    bool ValidateOrder(Order& order)
    {
        if (!order.VerifySignature())
        {
            log_ << "Signature verification failed!\n";
            return false;
        }
        log_ << "ValidateOrder(): signature: " << order.VerifySignature()
             << "\npubkeyauth: " << order.CheckPubKeyAuthorization() << "\n";
        bool result = order.VerifySignature() && 
                      order.CheckPubKeyAuthorization() &&
                      (order.proof_of_work.Length() == 0 || CheckWork(order));
        log_ << "validate order: " << result << "\n";
        return result;
    }

    void HandleOrder(Order order);

    template<typename MSG>
    void ScheduleForwarding(MSG& order_or_accept, uint160 hash)
    {
        should_forward /* immediately */ = false;

        uint160 work;
        if (order_or_accept.proof_of_work.Length() == 0)
            work = 0;
        else
        {
            uint160 numerator = 1;
            numerator <<= 128;
            work = numerator / order_or_accept.proof_of_work.target;
        }
        log_ << "scheduling forwarding for " << hash 
             << " with work " << work << "\n";
        tradedata[hash].Location("forwarding")
            = Priority(work, GetTimeMicros());
    }

    bool ValidateCancelOrder(CancelOrder cancel)
    {
        return cancel.VerifySignature();
    }

    void HandleCancelOrder(CancelOrder cancel)
    {
        if (!ValidateCancelOrder(cancel))
        {
            log_ << "Failed to validate cancel order\n";
            should_forward = false;
            return;
        }
        uint160 order_hash = cancel.order_hash;
        tradedata[order_hash]["cancelled"] = true;
        tradedata[order_hash]["cancel"] = cancel;
        marketdata.Delete(order_hash);
        log_ << "order " << order_hash << " cancelled\n";
    }

    bool ValidateAcceptOrder(AcceptOrder accept_order)
    {
        return accept_order.VerifySignature() &&
               (accept_order.proof_of_work.Length() == 0 ||
                CheckWork(accept_order));
    }

    void HandleAcceptOrder(AcceptOrder accept_order)
    {
        uint160 accept_order_hash = accept_order.GetHash160();
        log_ << "handling:" << accept_order;
        
        if (!ValidateAcceptOrder(accept_order))
        {
            log_ << "Failed to validate accept order\n";
            should_forward = false;
            return;
        }
        log_ << "accept_order.fund_address_pubkey = "
             << accept_order.fund_address_pubkey << "\n";
        log_ << "stored accept order in database\n";
        if (tradedata[accept_order.order_hash]["is_mine"])
        {
            log_ << "mine - handling\n";
            HandleAcceptMyOrder(accept_order);
        }
        ScheduleForwarding(accept_order, accept_order_hash);
    }

    bool ValidateOrderCommit(OrderCommit order_commit)
    {
        if (!order_commit.distributed_trade_secret.Validate()   ||
            !order_commit.VerifySignature())
        {
            log_ << "number of relays in secret: "
                 << order_commit.distributed_trade_secret.Relays().size()
                 << "\n"
                 << "validate trade secret: "
                 << order_commit.distributed_trade_secret.Validate() << "\n"
                 << "verify signature: " 
                 << order_commit.VerifySignature() << "\n";
            return false;
        }
        return true;
    }

    void HandleOrderCommit(OrderCommit order_commit)
    {
        log_ << "handling order commit:" << order_commit;
        if (!ValidateOrderCommit(order_commit))
        {
            should_forward = false;
            log_ << "not validated!\n";
            return;
        }
        
        if (tradedata[order_commit.accept_order_hash]["is_mine"])
        {
            log_ << "mine - handling\n";
            HandleCommitToOrderIAccepted(order_commit);
        }
        else
        {
            log_ << "not mine\n";
        }
    }

    bool ValidateAcceptCommit(AcceptCommit accept_commit)
    {
        if (!accept_commit.distributed_trade_secret.Validate()   ||
            !accept_commit.VerifySignature())
            return false;
        return true;
    }

    void HandleAcceptCommit(AcceptCommit accept_commit);

    void SendDisclosure(uint160 accept_commit_hash,
                        AcceptCommit& accept_commit,
                        Order& order,
                        bool is_response);

    void ValidateAndAcknowledgeTrade(AcceptCommit accept_commit);

    bool CheckDisclosure(SecretDisclosureMessage msg);

    void HandleSecretDisclosureMessage(SecretDisclosureMessage msg);

    void HandleAcceptMyOrder(AcceptOrder accept_order);

    void HandleCommitToOrderIAccepted(OrderCommit order_commit);
    
    void ScheduleTradeInitiation(uint160 accept_commit_hash);
    
    void HandleInitiateTrade(AcceptCommit accept_commit);

    void ProceedWithCreditPayment(AcceptCommit accept_commit, Order order);

    void ProceedWithCurrencyPayment(uint160 accept_commit_hash);

    void ProceedWithCurrencyPayment(AcceptCommit accept_commit, Order order);

    bool CheckForMyRelaySecrets(AcceptCommit accept_commit);

    bool ValidateAndStoreSecrets(DistributedTradeSecret secret, 
                                 uint160 message_hash);

    void HandleCurrencyPaymentConfirmation(CurrencyPaymentConfirmation 
                                           confirmation)
    {
        log_ << "handling:" << confirmation;
        ::HandleCurrencyPaymentConfirmation(confirmation);
    }

    void HandleThirdPartyTransactionAcknowledgement(
        ThirdPartyTransactionAcknowledgement acknowledgement)
    {
        log_ << "handling:" << acknowledgement;
        uint160 accept_commit_hash = acknowledgement.accept_commit_hash;
        
        if (!acknowledgement.VerifySignature())
        {
            log_ << "Signature check failed!\n";
            should_forward = false;
            return;
        }
        if (tradedata[accept_commit_hash].HasProperty("acknowledgement_hash"))
        {
            log_ << accept_commit_hash << " has already been acknowledged\n";
            should_forward = false;
            return;
        }
        tradedata[accept_commit_hash]["acknowledgement_hash"]
            = acknowledgement.GetHash160();
        log_ << accept_commit_hash << " has been acknowledged by ttp\n";
        tradedata[accept_commit_hash]["acknowledged"] = true;
    }

    void HandleThirdPartyPaymentConfirmation(ThirdPartyPaymentConfirmation
                                             confirmation)
    {
        log_ << "handling:" << confirmation;
        if (!confirmation.VerifySignature())
        {
            log_ << "Signature check failed!\n";
            return;
        }
        uint160 accept_commit_hash = confirmation.accept_commit_hash;
        if (tradedata[accept_commit_hash]["my_buy"])
        {
            log_ << "third party payment confirmation received\n"
                 << "releasing funds\n";
            ReleaseCreditToCounterParty(accept_commit_hash);
        }
        if (tradedata[accept_commit_hash]["i_have_secret"])
        {
            tradedata[accept_commit_hash]["currency_paid"] = true;
            tradedata[accept_commit_hash]["confirmation_time"]
                = GetTimeMicros();
        }

        tradedata[accept_commit_hash]["confirmation"] = confirmation;
    }

    bool CheckCounterpartySecretMessage(CounterpartySecretMessage msg)
    {
        return msg.VerifySignature();
    }

    void HandleCounterpartySecretMessage(CounterpartySecretMessage msg)
    {
        log_ << "HandleCounterpartySecretMessage: " << msg;
        if (!CheckCounterpartySecretMessage(msg))
        {
            log_ << "Bad counterparty secret message\n";
            should_forward = false;
            return;
        }
        uint160 hash = msg.accept_commit_hash;

        if (msg.sender_side == BID)
            tradedata[hash]["bid_counterparty_secret_sent"] = true;
        else
            tradedata[hash]["ask_counterparty_secret_sent"] = true;

        if (tradedata[hash]["my_buy"] || tradedata[hash]["my_sell"] 
                                      || tradedata[hash]["my_fiat_sell"])
        {
            log_ << "Mine - handling\n";
            HandleMyCounterpartySecretMessage(msg);
        }

        RecordTrade(hash);

        uint32_t counterparty_secrets_sent = tradedata[hash]["cps_sent"];
        counterparty_secrets_sent += 1;
        tradedata[hash]["cps_sent"] = counterparty_secrets_sent;

        if (counterparty_secrets_sent == 2)
            tradedata[hash]["awaited_relays"] = std::vector<Point>();
    }

    void HandleMyCounterpartySecretMessage(CounterpartySecretMessage msg)
    {
        log_ << "HandleMyCounterpartySecretMessage()\n";
        AcceptCommit accept_commit;
        OrderCommit commit;

        accept_commit= msgdata[msg.accept_commit_hash]["accept_commit"];
        commit = msgdata[accept_commit.order_commit_hash]["commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];

        uint8_t curve = (msg.sender_side == BID) 
                        ? SECP256K1 : accept_commit.curve;

        CBigNum shared_secret, privkey;
        Point my_pubkey, key, secret_pubkey;

        if (tradedata[msg.accept_commit_hash]["my_buy"] &&
            msg.sender_side == ASK)
        {
            log_ << "I'm the buyer receiving the message from the seller\n";
            if (order.side == BID)
                my_pubkey = order.VerificationKey();
            else
                my_pubkey = accept_commit.VerificationKey();
        }
        else if (tradedata[msg.accept_commit_hash]["my_sell"] &&
                 msg.sender_side == BID)
        {
            log_ << "I'm the seller receiving the message from the buyer\n";
            if (order.side == ASK)
                my_pubkey = order.VerificationKey();
            else
                my_pubkey = accept_commit.VerificationKey();
        }
        else
            return;
        privkey = keydata[my_pubkey]["privkey"];
#ifndef _PRODUCTION_BUILD
        log_ << "retrieved my privkey: " << privkey << "\n";
#endif

        DistributedTradeSecret secret;
        if (msg.VerificationKey() == accept_commit.VerificationKey())
            secret = accept_commit.distributed_trade_secret;
        else
            secret = commit.distributed_trade_secret;

        secret_pubkey = (msg.sender_side == BID) ? secret.CreditPubKey() 
                                                 : secret.CurrencyPubKey();
        shared_secret = Hash(privkey * secret_pubkey);

        bool ok = true;

        if (Point(secret_pubkey.curve,
                  shared_secret ^ msg.secret_xor_shared_secret)
            != secret_pubkey)
        {
            log_ << "secret recovery failed!\n";
            ok = false;
        }
        else
        {
            key = ForwardPubKeyOfDistributedSecret(msg.accept_commit_hash, 
                                                   msg.sender_side);
            if (key != Point(curve, shared_secret ^ msg.secret_xor_shared_secret))
            {
                log_ << "recovered secret: " << shared_secret
                     << " does not match forward pubkey: "
                     << key << "\n"
                     << "instead, it gives the point: "
                     << Point(curve, 
                              shared_secret ^ msg.secret_xor_shared_secret) 
                     << "\n";
                ok = false;
            }
        }

        if (!ok)
        {
            CounterpartySecretComplaint complaint(msg.accept_commit_hash,
                                                  msg.sender_side);
            BroadcastMessage(complaint);
            return;
        }
        keydata[key]["privkey"] = shared_secret ^ msg.secret_xor_shared_secret;
#ifndef _PRODUCTION_BUILD
        log_ << "successfully recovered: " 
             << (shared_secret ^ msg.secret_xor_shared_secret) << "\n";
#endif
        CompleteTransactionAfterRecoveringKey(msg.accept_commit_hash,
                                              msg.sender_side,
                                              FORWARD);
    }

    void HandleCounterpartySecretComplaint(CounterpartySecretComplaint msg)
    {
        if (!msg.VerifySignature())
            return;

        CounterpartySecretMessage secret_msg
            = msgdata[msg.secret_message_hash]["counterparty_secret"];

        uint160 accept_commit_hash = secret_msg.accept_commit_hash;
        if (tradedata[accept_commit_hash]["i_sent_secrets"] ||
            tradedata[accept_commit_hash]["cancelled"])
            return;

        uint160 msg_hash = msg.GetHash160();
        tradedata[accept_commit_hash]["counterparty_complaint"] = msg_hash;
        
        if (tradedata[accept_commit_hash]["payment_confirmation_received"]
            && CheckForCreditPayment(accept_commit_hash))
        {
            SendSecrets(accept_commit_hash, FORWARD);
        }
    }

    void HandleTradeSecretMessage(TradeSecretMessage message);

    void HandleBackupTradeSecretMessage(BackupTradeSecretMessage message);

    bool CheckRelayComplaint(RelayComplaint complaint)
    {
        return complaint.Validate() && complaint.VerifySignature();
    }

    void HandleRelayComplaint(RelayComplaint complaint)
    {
        uint160 message_hash = complaint.message_hash;
        uint160 complaint_hash = complaint.GetHash160();
        if (!CheckRelayComplaint(complaint))
        {
            should_forward = false;
            return;
        }
        
        if (tradedata[message_hash]["is_mine"])
        {
            RefutationOfRelayComplaint refutation(complaint_hash);
            BroadcastMessage(refutation);
        }

        string_t type = msgdata[message_hash]["type"];

        uint160 accept_commit_hash;
        if (type == "commit")
        {
            accept_commit_hash = msgdata[message_hash]["accept_commit"];
        }
        else if (type == "disclosure")
        {
            SecretDisclosureMessage msg = msgdata[message_hash]["disclosure"];
            accept_commit_hash = msg.accept_commit_hash;
        }
        else
            accept_commit_hash = message_hash;

        tradedata[accept_commit_hash]["cancelled"] = true;
    }

    void HandleRefutationOfRelayComplaint(RefutationOfRelayComplaint 
                                          refutation);

    bool CheckTraderComplaint(TraderComplaint complaint)
    {
        return complaint.VerifySignature();
    }

    void HandleTraderComplaint(TraderComplaint complaint);

    void HandleRefutationOfTraderComplaint(RefutationOfTraderComplaint 
                                           refutation);

    void HandleNewRelayChoiceMessage(NewRelayChoiceMessage msg)
    {
        ::HandleNewRelayChoiceMessage(msg);
    }

    void HandleTraderLeaveMessage(TraderLeaveMessage msg)
    {
        if (!msg.VerifySignature())
        {
            should_forward = false;
            return;
        }
        foreach_(uint160 order_hash, msg.order_hashes)
            marketdata.Delete(order_hash);
    }
};


#endif
