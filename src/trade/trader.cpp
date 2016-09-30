#include "trade/trade.h"
#include "teleportnode/teleportnode.h"

#include "log.h"
#define LOG_CATEGORY "trader.cpp"

extern vch_t TCR;

Point GetTraderPubKey(AcceptCommit accept_commit, uint8_t side)
{
    Order order = msgdata[accept_commit.order_hash]["order"];
    if (order.side == side)
        return order.VerificationKey();
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    AcceptOrder accept = msgdata[commit.accept_order_hash]["accept_order"];
    return accept.VerificationKey();
}


pair<uint8_t,vch_t> BidSide(vch_t currency)
{
    return std::make_pair(BID, currency);
}

pair<uint8_t,vch_t> AskSide(vch_t currency)
{
    return std::make_pair(ASK, currency);
}


void RememberPrivateKey(uint8_t curve, CBigNum privkey)
{
    Point pubkey(curve, privkey);
    keydata[pubkey]["privkey"] = privkey;
}

CBigNum RandomPrivateKey(uint8_t curve)
{
    CBigNum privkey;
    privkey.Randomize(Point(curve).Modulus());
    return privkey;
}

vch_t ProofFromCreditInBatch(CreditInBatch credit)
{
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << credit;
    return vch_t(ss.begin(), ss.end());
}

bool GetFiatPubKeyAndProof(vch_t currency_code,
                           uint64_t size, 
                           vch_t &fund_proof, 
                           Point &order_pubkey)
{
    log_ << "GetFiatPubKeyAndProof()\n";
    string address, proof_string;

    Currency currency = teleportnode.currencies[currency_code];
    bool result = currency.GetAddressAndProofOfFunds(address,
                                                     proof_string, 
                                                     size);
    
    fund_proof = vch_t(proof_string.begin(), proof_string.end());
    order_pubkey = teleportnode.wallet.NewKey();
    return result;
}

bool GetCryptoCurrencyPubKeyAndProof(vch_t currency_code,
                                     uint64_t size,
                                     vch_t &fund_proof,
                                     Point &order_pubkey,
                                     Point &fund_address_pubkey,
                                     Signature &pubkey_authorization)
{
    string address, proof_string;
    Currency currency = teleportnode.currencies[currency_code];
    bool success = teleportnode.currencies[currency_code].GetAddressAndProofOfFunds(
                                                      address, 
                                                      proof_string, 
                                                      size);
    log_ << "GetCryptoCurrencyPubKeyAndProof(): " << proof_string << "\n";
    fund_proof = vch_t(proof_string.begin(), proof_string.end());
    log_ << "privkey = " 
         << currency.crypto.GetPrivateKey(address) << "\n";

    CBigNum privkey = currency.crypto.GetPrivateKey(address);
    order_pubkey = Point(SECP256K1, privkey);

    keydata[order_pubkey]["privkey"] = privkey;

    uint256 order_pubkey_hash = Hash(order_pubkey.getvch());

    fund_address_pubkey =  Point(currency.crypto.curve, privkey);
    
    log_ << "curve is " << currency.crypto.curve
         << " order_pubkey_hash = " << order_pubkey_hash
#ifndef _PRODUCTION_BUILD
         << " privkey = " << privkey 
#endif
         << "\n";
    if (currency.crypto.curve != SECP256K1)
        pubkey_authorization = SignHashWithKey(order_pubkey_hash, 
                                               privkey,
                                               currency.crypto.curve);

    log_ << "address = " << address << "\n";
    log_ << "pubkey = " << order_pubkey << "\n";
    return success;
}

bool GetCreditPubKeyAndProof(uint64_t amount,
                             Point &order_pubkey,
                             vch_t &fund_proof,
                             Point &fund_address_pubkey)
{
    log_ << "GetCreditPubKeyAndProof()\n";
    CreditInBatch credit = teleportnode.wallet.GetCreditWithAmount(amount);
    if (credit.amount < amount || credit.keydata.size() == 0)
    {
        log_ << "failed to get fund address pubkey & proof\n";
        return false;
    }
    if (credit.keydata.size() == 20)
        order_pubkey = keydata[uint160(credit.keydata)]["pubkey"];
    else
        order_pubkey.setvch(credit.keydata);
    
    fund_proof = ProofFromCreditInBatch(credit);
    log_ << "length of proof is " << fund_proof.size() << "\n";
    fund_address_pubkey = order_pubkey;
    return true;
}



uint160 PlaceOrder(vch_t currency_code, 
                   uint8_t side, 
                   uint64_t size, 
                   uint64_t integer_price,
                   uint64_t difficulty,
                   string_t auxiliary_data)
{
    Currency currency = teleportnode.currencies[currency_code];

    if (auxiliary_data.size() == 0)
        auxiliary_data = CurrencyInfo(currency_code, "auxiliary_data");

    Order order(currency_code, side, size, integer_price, auxiliary_data);

    if (currency_code == TCR)
        return 0;
    
    vch_t funds_currency = order.side == BID ? TCR : currency_code;

    if (order.is_cryptocurrency && !order.successfully_funded)
    {
        log_ << "Failed to fund order!\n";
        return 0;
    }

    if (difficulty > 0)
    {
        uint256 prework_hash = order.GetHash();
        log_ << "doing work for order\n";
        uint160 target(1);
        target <<= 128;
        target = target / difficulty;
        uint160 link_threshold = target * 64;
        order.proof_of_work = TwistWorkProof(prework_hash,
                                             15, // 0.25 Gb
                                             target,
                                             link_threshold,
                                             32);
        uint8_t keep_going = 1;
        order.proof_of_work.DoWork(&keep_going);
    }
    
    order.Sign();
    if (!order.VerifySignature())
    {
        log_ << "order signed but signature verification failed\n";
    }
    uint160 order_hash = order.GetHash160();

    tradedata[order_hash]["is_mine"] = true;
    teleportnode.tradehandler.BroadcastMessage(order);
    tradedata[order_hash].Location("my_orders") = GetTimeMicros();

    return order_hash;
}

uint160 SendAccept(Order order, uint64_t difficulty, string_t auxiliary_data)
{
    AcceptOrder accept_order(order, auxiliary_data);
    
    vch_t funds_currency = order.side == BID ? order.currency : TCR;

    log_ << "SendAccept(): funds_currency = "
         << string(funds_currency.begin(), funds_currency.end()) << "\n";
    log_ << "SendAccept(): order.currency = "
         << string(order.currency.begin(), order.currency.end()) << "\n";

    if (!accept_order.successfully_funded)
    {
        log_ << "failed to get fund_address_pubkey and proof\n";
        return 0;
    }
    log_ << "SendAccept(): pubkey = "
         << accept_order.fund_address_pubkey << "\n";

    if (difficulty > 0)
    {
        uint256 prework_hash = accept_order.GetHash();
        log_ << "doing work for order\n";
        uint64_t target = (1ULL << 63) / (difficulty / 2);
        uint64_t link_threshold = target * 64;
        accept_order.proof_of_work = TwistWorkProof(prework_hash,
                                                    15, // 0.25 Gb
                                                    target,
                                                    link_threshold,
                                                    32);
        uint8_t keep_going = 1;
        accept_order.proof_of_work.DoWork(&keep_going);
    }

    accept_order.Sign();
    uint160 accept_order_hash = accept_order.GetHash160();
    tradedata[accept_order_hash]["is_mine"] = true;
    log_ << "broadcasting accept order\n";
    teleportnode.tradehandler.BroadcastMessage(accept_order);
    return accept_order_hash;
}


bool CheckAndStoreSecretsFromCommit(OrderCommit commit)
{
    AcceptOrder accept = msgdata[commit.accept_order_hash]["accept_order"];
    CBigNum privkey = keydata[accept.order_pubkey]["privkey"];
#ifndef _PRODUCTION_BUILD
    log_ << "CheckAndStoreSecretsFromCommit(): privkey is "
         << privkey << "\n";
#endif
    log_ << "CheckAndStoreSecretsFromCommit(): commit.shared_credit_pubkey is "
         << commit.shared_credit_pubkey << "\n";
    CBigNum shared_secret = Hash(privkey * commit.shared_credit_pubkey);
#ifndef _PRODUCTION_BUILD
    log_ << "CheckAndStoreSecretsFromCommit(): shared secret is "
         << shared_secret << "\n";
#endif

    CBigNum shared_credit_secret
        = shared_secret ^ commit.shared_credit_secret_xor_shared_secret;

#ifndef _PRODUCTION_BUILD
    log_ << "shared credit secret = " << shared_credit_secret << "\n";
#endif

    CBigNum shared_currency_secret
        = Hash(shared_credit_secret)
            ^ commit.shared_currency_secret_xor_hash_of_shared_credit_secret;

    if (Point(SECP256K1, shared_credit_secret) != commit.shared_credit_pubkey)
    {
        log_ << "bad secret: " << Point(SECP256K1, shared_credit_secret)
             << " != " << commit.shared_credit_pubkey << "\n";
        return false;
    }
    if (Point(commit.curve, shared_currency_secret)
            != commit.shared_currency_pubkey)
    {
        log_ << "bad secret: " << Point(commit.curve, shared_currency_secret)
             << " != " << commit.shared_currency_pubkey << "\n";
        return false;
    }

    keydata[commit.shared_credit_pubkey]["privkey"] = shared_credit_secret;
    keydata[commit.shared_currency_pubkey]["privkey"] = shared_currency_secret;
    log_ << "secrets from commit ok\n";
    return true;
}

Point GetEscrowCreditPubKey(OrderCommit order_commit, 
                            AcceptCommit accept_commit)
{
    return order_commit.distributed_trade_secret.CreditPubKey()
           + order_commit.shared_credit_pubkey
           + accept_commit.distributed_trade_secret.CreditPubKey();
}

Point GetEscrowCreditPubKey(uint160 accept_commit_hash)
{
    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    return GetEscrowCreditPubKey(commit, accept_commit);
}

CBigNum GetEscrowCreditPrivKey(OrderCommit order_commit, 
                               AcceptCommit accept_commit)
{
    Point credit_pubkey = order_commit.distributed_trade_secret.CreditPubKey();
    CBigNum order_commit_credit_privkey = keydata[credit_pubkey]["privkey"];

    credit_pubkey = accept_commit.distributed_trade_secret.CreditPubKey();
    CBigNum accept_commit_credit_privkey = keydata[credit_pubkey]["privkey"];
    
    CBigNum order_commit_credit_shared_privkey
        = keydata[order_commit.shared_credit_pubkey]["privkey"];

#ifndef _PRODUCTION_BUILD
    log_ << "order_commit_credit_privkey = "
         << order_commit_credit_privkey << "\n"
         << "accept_commit_credit_privkey = "
         << accept_commit_credit_privkey << "\n"
         << "order_commit_credit_shared_privkey = "
         << order_commit_credit_shared_privkey << "\n";
#endif

    return (order_commit_credit_privkey
            + accept_commit_credit_privkey
            + order_commit_credit_shared_privkey)
            % Point(SECP256K1, 0).Modulus();
}

CBigNum GetEscrowCreditPrivKey(uint160 accept_commit_hash)
{
    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    return GetEscrowCreditPrivKey(commit, accept_commit);
}


Point GetEscrowCurrencyPubKey(OrderCommit order_commit, 
                              AcceptCommit accept_commit)
{
    return order_commit.distributed_trade_secret.CurrencyPubKey()
           + accept_commit.distributed_trade_secret.CurrencyPubKey()
           + order_commit.shared_currency_pubkey;   
}

Point GetEscrowCurrencyPubKey(uint160 accept_commit_hash)
{
    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    return GetEscrowCurrencyPubKey(commit, accept_commit);
}

CBigNum GetEscrowCurrencyPrivKey(OrderCommit commit, 
                                 AcceptCommit accept_commit)
{
    Point currency_pubkey = commit.distributed_trade_secret.CurrencyPubKey();
    CBigNum commit_currency_privkey = keydata[currency_pubkey]["privkey"];
    
    currency_pubkey = accept_commit.distributed_trade_secret.CurrencyPubKey();
    CBigNum accept_commit_currency_privkey
        = keydata[currency_pubkey]["privkey"];
    
    CBigNum commit_currency_shared_privkey
        = keydata[commit.shared_currency_pubkey]["privkey"];

#ifndef _PRODUCTION_BUILD
    log_ << "GetEscrowCurrencyPrivKey():\n"
         << "commit_currency_privkey = " << commit_currency_privkey << "\n"
         << "accept_commit_currency_privkey = " 
         << accept_commit_currency_privkey << "\n"
         << "commit_currency_shared_privkey = "
         << commit_currency_shared_privkey << "\n";
#endif

    return (commit_currency_privkey
            + accept_commit_currency_privkey
            + commit_currency_shared_privkey)
            % Point(commit.curve).Modulus();
}

CBigNum GetEscrowCurrencyPrivKey(uint160 accept_commit_hash)
{
    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    return GetEscrowCurrencyPrivKey(commit, accept_commit);
}
std::vector<Point> ChooseRelays(uint160 chooser,
                                uint160 credit_hash,
                                uint32_t num_relays)
{
    std::vector<Point> relays;

    RelayState state = GetRelayState(credit_hash);

    log_ << "Choosing relays with chooser " << chooser
         << " from state " << state.GetHash160() << "\n";
    
    return state.ChooseRelays(chooser, num_relays);
}

std::vector<Point> ChooseRelays(uint160 relay_chooser1, 
                                uint160 relay_chooser2,
                                uint160 credit_hash,
                                uint32_t num_relays)
{
    uint160 chooser = relay_chooser1 ^ relay_chooser2;
    return ChooseRelays(chooser, credit_hash, num_relays);
}

void SendCurrencyPaymentProof(uint160 accept_commit_hash, vch_t proof)
{
    CurrencyPaymentProof payment_proof(accept_commit_hash, proof);
    
    payment_proof.Sign();
    log_ << "Sending CurrencyPaymentProof! hash = "
         << payment_proof.GetHash160() << "\n";
    teleportnode.tradehandler.BroadcastMessage(payment_proof);
}

bool CheckCurrencyPaymentProof(CurrencyPaymentProof payment_proof)
{
    if (tradedata[payment_proof.accept_commit_hash].HasProperty("proof"))
        return false;
    return true;
}


void ScheduleSecretsCheck(uint160 accept_commit_hash, uint32_t seconds)
{
    teleportnode.scheduler.Schedule("secrets_check", accept_commit_hash,
                                GetTimeMicros() + seconds * 1000 * 1000);
}


DistributedTradeSecret 
GetSecretThatShouldBeSentToMe(uint160 accept_commit_hash, uint8_t my_side)
{
    AcceptCommit accept_commit;
    accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    
    OrderCommit order_commit;
    order_commit = msgdata[accept_commit.order_commit_hash]["commit"];

    uint160 order_hash = accept_commit.order_hash;
    Order order = msgdata[order_hash]["order"];
    
    if (my_side == order.side)
        return accept_commit.distributed_trade_secret;
    else
        return order_commit.distributed_trade_secret;
    
    return DistributedTradeSecret();
}

Point ForwardPubKeyOfDistributedSecret(uint160 accept_commit_hash, 
                                       uint8_t sender_side)
{
    log_ << "ForwardPubKeyOfDistributedSecret()\n";

    AcceptCommit accept_commit;
    accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    
    OrderCommit order_commit;
    order_commit = msgdata[accept_commit.order_commit_hash]["commit"];

    uint160 order_hash = accept_commit.order_hash;
    Order order = msgdata[order_hash]["order"];
    
    if (order.side == sender_side && sender_side == BID)
    {
        log_ << "bidder sending to order accepter: pubkey is: "
             << order_commit.distributed_trade_secret.CreditPubKey() << "\n";
        return order_commit.distributed_trade_secret.CreditPubKey();
    }
    else if (order.side == sender_side && sender_side == ASK)
    {
        log_ << "asker sending to order accepter: pubkey is: "
             << order_commit.distributed_trade_secret.CurrencyPubKey() << "\n";
        return order_commit.distributed_trade_secret.CurrencyPubKey();
    }

    else if (order.side != sender_side && sender_side == BID)
    {
        log_ << "bidder sending to order placer: pubkey is: "
             << accept_commit.distributed_trade_secret.CreditPubKey() << "\n";
        log_ << "accept commit is: " << accept_commit 
             << "distributed trade secret is" 
             << accept_commit.distributed_trade_secret;
        return accept_commit.distributed_trade_secret.CreditPubKey();
    }
    else  if (order.side != sender_side && sender_side == ASK)
    {
        log_ << "asker sending to order placer: pubkey is: "
             << accept_commit.distributed_trade_secret.CurrencyPubKey() << "\n";
        return accept_commit.distributed_trade_secret.CurrencyPubKey();
    }

    return Point(SECP256K1);
}



void ChooseNewRelays(uint160 accept_commit_hash)
{
    log_ << "ChooseNewRelays()\n";
    if (tradedata[accept_commit_hash]["new_relays_chosen"])
    {
        uint64_t relay_choice_time
            = msgdata[accept_commit_hash]["new_relay_choice_time"];
        if (GetTimeMicros() - relay_choice_time < 12 * COMPLAINT_WAIT_TIME)
            return;
    }
    uint160 credit_hash = teleportnode.previous_mined_credit_hash;
    uint8_t side;

    if (tradedata[accept_commit_hash]["my_buy"])
        side = BID;
    else
        side = ASK;
    NewRelayChoiceMessage msg(accept_commit_hash, credit_hash, side);
    teleportnode.tradehandler.BroadcastMessage(msg);
    tradedata[accept_commit_hash]["new_relays_req_ask"] = true;
}

void DoScheduledSecretsCheck(uint160 accept_commit_hash)
{
    if (tradedata[accept_commit_hash]["completed"] ||
        tradedata[accept_commit_hash]["cancelled"])
        return;

    uint64_t now = GetTimeMicros();

    log_ << "doing scheduled secrets check\n";
    log_ << "checking number of secrets for " << accept_commit_hash << "\n";
    uint32_t num_secrets_received = tradedata[accept_commit_hash]["n_secrets"];
    log_ << "number of secrets received: " << num_secrets_received << "\n";

    if (num_secrets_received < TRADE_SECRETS_NEEDED)
    {
        uint64_t currency_payment_time
            = tradedata[accept_commit_hash]["confirmation_time"];
        uint64_t credit_payment_time
            = tradedata[accept_commit_hash]["credit_pay_time"];

        if (currency_payment_time && credit_payment_time)
        {
            uint64_t waiting_for_secrets_since
                = currency_payment_time > credit_payment_time
                  ? currency_payment_time : credit_payment_time;

            if (now - waiting_for_secrets_since > CHOOSE_NEW_RELAYS_AFTER)
            {        
                ChooseNewRelays(accept_commit_hash);
            }
        }
        ScheduleSecretsCheck(accept_commit_hash, 10);
    }
}

void DoScheduledTradeInitiation(uint160 accept_commit_hash)
{
    log_ << "DoScheduledTradeInitiation: " << accept_commit_hash << "\n";

    pair<string_t, uint8_t> bid_disclosure, ask_disclosure;

    bid_disclosure = make_pair(string_t("disclosure"), BID);
    ask_disclosure = make_pair(string_t("disclosure"), ASK);

    if (!tradedata[accept_commit_hash].HasProperty(bid_disclosure))
    {
        log_ << "bid disclosure not received - aborting\n";
        return;
    }
    if (!tradedata[accept_commit_hash].HasProperty(ask_disclosure))
    {
        log_ << "ask disclosure not received - aborting\n";
        return;
    }
    if (tradedata[accept_commit_hash]["cancelled"])
    {
        log_ << "trade " << accept_commit_hash << " was cancelled\n";
        return;
    }
    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    if (tradedata[commit.GetHash160()]["cancelled"])
    {
        log_ << "trade " << commit.GetHash160() << " was cancelled\n";
        return;
    }
    teleportnode.tradehandler.HandleInitiateTrade(accept_commit);
}


void SendCurrencyPaymentConfirmation(uint160 accept_commit_hash,
                                     uint160 payment_hash)
{
    CurrencyPaymentConfirmation confirmation(accept_commit_hash,
                                             payment_hash);
    confirmation.Sign();

    teleportnode.tradehandler.BroadcastMessage(confirmation);
    ScheduleSecretsCheck(accept_commit_hash, 15);
}

bool CheckPaymentConfirmation(CurrencyPaymentConfirmation confirmation)
{
    log_ << "CheckPaymentConfirmation()\n";
    if (confirmation.affirmation != PAYMENT_CONFIRMED)
    {
        log_ << "wrong affirmation!\n";
        return false;
    }
    uint160 accept_commit_hash = confirmation.accept_commit_hash;
    AcceptCommit accept_commit;
    accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    if (accept_commit.order_hash != confirmation.order_hash)
    {
        log_ << "accept_commit.order_hash != confirmation.order_hash\n"
             << accept_commit.order_hash << " != "
             << confirmation.order_hash << "\n";
        return false;
    }
    return confirmation.VerifySignature();
}

void ScheduleCreditPaymentCheck(uint160 accept_commit_hash,
                                uint32_t seconds = 5)
{
    teleportnode.scheduler.Schedule("credit_payment_check", accept_commit_hash,
                                GetTimeMicros() + seconds * 1000 * 1000);
}

void DoScheduledCreditPaymentCheck(uint160 accept_commit_hash)
{
    log_ << "doing scheduled credit payment check: "
         << accept_commit_hash << "\n";

    if (!CheckForCreditPayment(accept_commit_hash))
    {
        log_ << "no credit payment yet - scheduling another check\n";
        ScheduleCreditPaymentCheck(accept_commit_hash);
        return;
    }
    log_ << "Credit payment made!\n";
    if (tradedata[accept_commit_hash]["my_fiat_sell"])
    {
        log_ << "my fiat sell\n";
        tradedata[accept_commit_hash]["credit_paid"] = true;
        teleportnode.tradehandler.ProceedWithCurrencyPayment(accept_commit_hash);
    }
    else if (tradedata[accept_commit_hash]["my_sell"])
    {
        log_ << "my sell\n";
        if (tradedata[accept_commit_hash]["payment_confirmation_received"])
        {
            log_ << "counterparty has sent payment confirmation.\n";
            log_ << "releasing funds.\n";
            tradedata[accept_commit_hash]["credit_paid"] = true;
            ReleaseCurrencyToCounterParty(accept_commit_hash);
        }
        else
        {
            log_ << "no payment confirmation yet - scheduling another check\n";
            ScheduleCreditPaymentCheck(accept_commit_hash);
        }
    }
}


void ReleaseCurrencyToCounterParty(uint160 accept_commit_hash)
{
    CounterpartySecretMessage msg(accept_commit_hash, ASK);
#ifndef TEST_RELAYS
    teleportnode.tradehandler.BroadcastMessage(msg);
#endif
}

void ReleaseCreditToCounterParty(uint160 accept_commit_hash)
{
    CounterpartySecretMessage msg(accept_commit_hash, BID);
#ifndef TEST_RELAYS
    teleportnode.tradehandler.BroadcastMessage(msg);
#endif
}


void SendTraderComplaint(uint160 message_hash, uint8_t side)
{
    TraderComplaint msg(message_hash, side);
    teleportnode.tradehandler.BroadcastMessage(msg);
}

void WatchEscrowCreditPubKey(Point escrow_key)
{
    vch_t keydata = escrow_key.getvch();
    uint160 keyhash = KeyHashFromKeyData(keydata);
    walletdata[keyhash]["watched"] = true;
    walletdata[keyhash].Location("generated") = GetTimeMicros();
}

void WatchEscrowCreditPubKey(uint160 accept_commit_hash)
{
    AcceptCommit accept_commit;
    accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    Point escrow_key = GetEscrowCreditPubKey(commit, accept_commit);
    return WatchEscrowCreditPubKey(escrow_key);
}

vch_t SendCredit(AcceptCommit accept_commit, Order order)
{
    log_ << "SendCredit()\n";
    OrderCommit order_commit;
    order_commit = msgdata[accept_commit.order_commit_hash]["commit"];
    Point escrow_key = GetEscrowCreditPubKey(order_commit, accept_commit);
    
    WatchEscrowCreditPubKey(escrow_key);

    uint64_t amount = (order.size * order.integer_price);

    bool trade_transaction = true;
    UnsignedTransaction rawtx
        = teleportnode.wallet.GetUnsignedTxGivenPubKey(escrow_key, amount, 
                                                   trade_transaction);
    
    SignedTransaction tx = SignTransaction(rawtx);

    teleportnode.HandleTransaction(tx, NULL);
    log_ << "sent " << amount << " to " << escrow_key << "\n";
    uint256 txhash = tx.GetHash();
    return vch_t(UBEGIN(txhash), UEND(txhash));
}

vch_t SendCurrency(AcceptCommit accept_commit, Order order)
{
    string payment_proof;
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    AcceptOrder accept = msgdata[commit.accept_order_hash]["accept_order"];
    
    Point escrow_credit_key = GetEscrowCreditPubKey(commit, accept_commit);
    
    WatchEscrowCreditPubKey(escrow_credit_key);

    Currency currency = teleportnode.currencies[order.currency];

    if (currency.is_cryptocurrency)
    {
        Point escrow_key = GetEscrowCurrencyPubKey(commit, accept_commit);
        payment_proof = currency.crypto.SendToPubKey(escrow_key, order.size);
    }
    else
    {
        vch_t my_data, their_data;
        if (order.side == ASK)
        {
            my_data = order.auxiliary_data;
            their_data = accept.auxiliary_data;
        }
        else
        {
            my_data = accept.auxiliary_data;
            their_data = order.auxiliary_data;
        }
        payment_proof = currency.SendFiat(my_data, their_data, order.size);
    }
    
    return vch_t(payment_proof.begin(), payment_proof.end());
}


template <typename MSG>
void HandleSecretMessage(MSG message)
{
    log_ << "HandleSecretMessage(): " << message;
    if (!message.VerifySignature())
    {
        log_ << "signature verification failed\n";
        teleportnode.tradehandler.should_forward = false;
        return;
    }
    Point relay = message.VerificationKey();

    pair<string_t, Point> key = make_pair("received_secret_from", relay);
    tradedata[message.accept_commit_hash][key] = true;

    Point original_relay;

    if (message.Type() == "backup_secret")
    {
        AcceptCommit accept_commit;
        accept_commit = msgdata[message.accept_commit_hash]["accept_commit"];
        uint32_t i = message.position;
        original_relay = accept_commit.distributed_trade_secret.Relays()[i];
    }
    else 
        original_relay = relay;
    
    vector<Point> awaited_relays = tradedata[message.accept_commit_hash]["awaited_relays"];
    EraseEntryFromVector(original_relay, awaited_relays);
    tradedata[message.accept_commit_hash]["awaited_relays"] = awaited_relays;

    if (message.direction == FORWARD)
    {
        log_ << "forward secret!\n";
        HandleSecret(message, FORWARD);
    }
    else
    {
        log_ << "backward secret!\n";
        HandleSecret(message, BACKWARD);
    }
    
    uint64_t task_time = relaydata[message.accept_commit_hash].Location(relay);
    relaydata.RemoveFromLocation(relay, task_time);
}

template <typename MSG>
void HandleSecret(MSG message, uint8_t direction)
{
    if (tradedata[message.accept_commit_hash]["my_buy"])
    {
        log_ << "Handling secret sent by seller\n";
        HandleSecret(message, ASK, direction);
    }
    if (tradedata[message.accept_commit_hash]["my_sell"])
    {
        log_ << "handling secret sent by buyer\n";
        HandleSecret(message, BID, direction);
    }
}

Point GetPubKeyOfPieceOfSecret(uint160 accept_commit_hash,
                               uint8_t side, // BID <=> credit
                               uint8_t direction,
                               uint32_t position)
{
    AcceptCommit accept_commit;
    accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    Order order = msgdata[commit.order_hash]["order"];

    DistributedTradeSecret secret;

    if ((side == order.side && direction == FORWARD) ||
            (side != order.side && direction == BACKWARD))
        secret = commit.distributed_trade_secret;
    else if ((side != order.side && direction == FORWARD) ||
             (side == order.side && direction == BACKWARD))
        secret = accept_commit.distributed_trade_secret;

    PieceOfSecret piece = secret.pieces[position];
    return side == BID ? piece.credit_pubkey : piece.currency_pubkey;
}


void RecordBroadcastOfSecret(uint160 accept_commit_hash, uint64_t position)
{
    vector<uint64_t> positions = tradedata[accept_commit_hash]["secrets_broadcast"];
    if (!VectorContainsEntry(positions, position))
        positions.push_back(position);
    tradedata[accept_commit_hash]["secrets_broadcast"] = positions;
}

template <typename MSG>
void HandleSecret(MSG message, uint8_t original_sender_side, uint8_t direction)
{
    log_ << "HandleSecret(): original sender side: " 
         << original_sender_side << "  direction:" << direction
         << message;

    uint160 message_hash = message.GetHash160();
    if (tradedata[make_pair(message_hash, original_sender_side)]["received"])
    {
        log_ << "Already have this secret\n";
        return;
    }
    tradedata[make_pair(message_hash, original_sender_side)]["received"] = true;

    uint160 accept_commit_hash = message.accept_commit_hash;
    RecordBroadcastOfSecret(accept_commit_hash, message.position);
    AcceptCommit accept_commit;
    accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    
    Point pubkey_of_secret = GetPubKeyOfPieceOfSecret(accept_commit_hash,
                                                      original_sender_side,
                                                      direction,
                                                      message.position);

    uint8_t recipient_side = original_sender_side == BID ? ASK : BID;

    Point recipient_pubkey = GetTraderPubKey(accept_commit, recipient_side);
    CBigNum recipient_privkey = keydata[recipient_pubkey]["privkey"];

    CBigNum shared_secret = Hash(recipient_privkey * pubkey_of_secret);
#ifndef _PRODUCTION_BUILD
    log_ << "recipient_pubkey = " << recipient_pubkey << "\n"
         << "recipient_privkey = " << recipient_privkey << "\n"
         << "pubkey_of_secret = " << pubkey_of_secret << "\n"
         << "shared_secret = " << shared_secret << "\n";
#endif

    CBigNum secret;

    if (original_sender_side == BID)
        secret = shared_secret ^ message.credit_secret_xor_shared_secret;
    else
        secret = shared_secret 
                 ^ message.currency_secret_xor_other_shared_secret;
    
    if (pubkey_of_secret != Point(pubkey_of_secret.curve, secret)) 
    {
        SendTraderComplaint(message_hash, original_sender_side);
        log_ << "pubkey of secret: " << pubkey_of_secret
             << " != " 
             << Point(pubkey_of_secret.curve, secret)
             << " which is Point(secret)\n";
        return;
    }

    keydata[pubkey_of_secret]["privkey"] = secret;
    
    uint32_t num_secrets_received
        = IncrementAndCountSecretsReceived(accept_commit_hash, 
                                           original_sender_side);

    log_ << "number of secrets received: " << num_secrets_received << "\n";
    if (num_secrets_received == TRADE_SECRETS_NEEDED)
    {
        log_ << "Threshold reached - completing transaction\n";
        DistributedTradeSecret dist_secret 
            = GetSecretThatShouldBeSentToMe(accept_commit_hash, 
                                            recipient_side);
        CompleteTransaction(accept_commit_hash, dist_secret, 
                            original_sender_side, direction);
    }
}

uint32_t IncrementAndCountSecretsReceived(uint160 accept_commit_hash,
                                          uint8_t side)
{
    pair<uint160, uint8_t> tradeside = make_pair(accept_commit_hash, side);

    tradedata[tradeside]["n_secrets"] = (uint32_t)
                                        tradedata[tradeside]["n_secrets"] + 1;

    return tradedata[tradeside]["n_secrets"];
}


void CompleteTransaction(uint160 accept_commit_hash,
                         DistributedTradeSecret dist_secret,
                         uint8_t sender_side,
                         uint8_t direction)
{
    CBigNum privkey;
    std::vector<CBigNum> secrets;
    bool cheated;
    std::vector<int32_t> cheaters;
    log_ << "CompleteTransaction(): " << accept_commit_hash << "\n"
         << "distributed secret is " << dist_secret;
    foreach_(const PieceOfSecret& piece, dist_secret.pieces)
    {
        Point pubkey = sender_side == BID ? piece.credit_pubkey 
                                          : piece.currency_pubkey;
        CBigNum secret = keydata[pubkey]["privkey"];
#ifndef _PRODUCTION_BUILD
        log_ << "adding secret " << secret << "\n";
#endif
        secrets.push_back(secret);
    }

    vector<Point> commitments = sender_side == BID 
                                 ? dist_secret.CreditPointValues()
                                 : dist_secret.CurrencyPointValues();

    log_ << "CompleteTransaction: " 
         << (sender_side == BID ? "credit" :"currency")
         << " commitments are: " << commitments << "\n";

    uint8_t curve = sender_side == BID ? SECP256K1 
                                : dist_secret.CurrencyPubKey().curve;

    generate_private_key_from_N_secret_parts(curve,
                                             privkey,
                                             secrets,
                                             commitments,
                                             secrets.size(),
                                             cheated,
                                             cheaters);
#ifndef _PRODUCTION_BUILD
    log_ << "recovered private key: " << privkey << "\n";
#endif
    Point pubkey(curve, privkey);
    log_ << "corresponding public key is " << pubkey << "\n";
    keydata[pubkey]["privkey"] = privkey;
    CompleteTransactionAfterRecoveringKey(accept_commit_hash, 
                                          sender_side, 
                                          direction);
}

void CompleteTransactionAfterRecoveringKey(uint160 accept_commit_hash,
                                           uint8_t sender_side,
                                           uint8_t direction)
{
    log_ << "CompleteTransactionAfterRecoveringKey: " << accept_commit_hash
         << "\n";
    CBigNum escrow_privkey;
    Point escrow_pubkey;

    if ((sender_side == ASK && direction == FORWARD) ||
        (sender_side == BID && direction == BACKWARD))
    {
        escrow_privkey = GetEscrowCurrencyPrivKey(accept_commit_hash);
        escrow_pubkey = GetEscrowCurrencyPubKey(accept_commit_hash);
    }
    else if ((sender_side == BID && direction == FORWARD) ||
             (sender_side == ASK && direction == BACKWARD))
    {
        escrow_privkey = GetEscrowCreditPrivKey(accept_commit_hash);
        escrow_pubkey = GetEscrowCreditPubKey(accept_commit_hash);
    }
#ifndef _PRODUCTION_BUILD
    log_ << "escrow private key is " << escrow_privkey << "\n";
#endif
    log_ << "escrow pubkey is " << escrow_pubkey << "\n";

    if (Point(escrow_pubkey.curve, escrow_privkey) == escrow_pubkey)
    {
        log_ << "matches escrow pubkey!\n";
        tradedata[accept_commit_hash]["completed"] = true;
    }
    else
    {
        log_ << "Something is wrong: pubkey does not match privkey\n";
    }

    ImportPrivKeyAfterTransaction(escrow_privkey, accept_commit_hash, 
                                  sender_side, direction);
}

void ImportPrivKeyAfterTransaction(CBigNum privkey,
                                   uint160 accept_commit_hash,
                                   uint8_t sender_side, 
                                   uint8_t direction)
{
#ifndef _PRODUCTION_BUILD
    log_ << "ImportPrivKeyAfterTransaction: privkey is " << privkey << "\n";
#endif
    if (direction == FORWARD && sender_side == BID)
    {
        log_ << "sale completed - importing wallet privkey\n";
        log_ << "balance before import: " << teleportnode.wallet.Balance() << "\n";
        teleportnode.wallet.ImportPrivateKey(privkey);
        log_ << "balance after import: " << teleportnode.wallet.Balance() << "\n";
        NotifyGuiOfBalance(TCR, teleportnode.wallet.Balance());
    }
    if (direction == BACKWARD && sender_side == ASK)
    {
        log_ << "purchase aborted - importing wallet privkey\n";
        log_ << "balance before import: " << teleportnode.wallet.Balance() << "\n";
        teleportnode.wallet.ImportPrivateKey(privkey);
        log_ << "balance after import: " << teleportnode.wallet.Balance() << "\n";
        NotifyGuiOfBalance(TCR, teleportnode.wallet.Balance());
    }
    if (direction == FORWARD && sender_side == ASK)
    {
        log_ << "purchase completed - importing currency key\n";
        AcceptCommit accept_commit;
        accept_commit = msgdata[accept_commit_hash]["accept_commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];
        vch_t currency_code = order.currency;
        Currency currency = teleportnode.currencies[currency_code];
        log_ << "balance before import: " << currency.Balance() << "\n";
        currency.crypto.ImportPrivateKey(privkey);
        log_ << "balance after import: " << currency.Balance() << "\n";
        NotifyGuiOfBalance(currency_code, currency.Balance());
    }
}


/*************************
 *  TradeHandler
 */

    void TradeHandler::HandleInitiateTrade(AcceptCommit accept_commit)
    {
        log_ << "Initiate trade!!\n";

        Order order = msgdata[accept_commit.order_hash]["order"];
        uint160 accept_commit_hash = accept_commit.GetHash160();

        if (tradedata[accept_commit_hash]["initiated"])
        {
            log_ << "Already initiated this trade\n";
            return;
        }

        if (tradedata[accept_commit.order_hash]["is_mine"])
        {
            if (order.side == BID)
                ProceedWithCreditPayment(accept_commit, order);
            else
                ProceedWithCurrencyPayment(accept_commit, order);
        }
        
        if (tradedata[accept_commit_hash]["is_mine"])
        {   
            if (order.side == BID)
                ProceedWithCurrencyPayment(accept_commit, order);
            else
                ProceedWithCreditPayment(accept_commit, order);
        }
        tradedata[accept_commit_hash]["initiated"] = true;
    }

    void TradeHandler::HandleTradeSecretMessage(
        TradeSecretMessage message)
    {
        HandleSecretMessage(message);
    }

    void TradeHandler::HandleBackupTradeSecretMessage(
        BackupTradeSecretMessage message)
    {
        HandleSecretMessage(message);
    }

    void TradeHandler::ProceedWithCreditPayment(AcceptCommit accept_commit, Order order)
    {
        log_ << "proceeding with credit payment!!\n";
        uint160 accept_commit_hash = accept_commit.GetHash160();
        tradedata[accept_commit_hash]["my_buy"] = true;
        log_ << "my buy: " << accept_commit_hash << "\n";
        SendCredit(accept_commit, order);
    }

    void TradeHandler::ProceedWithCurrencyPayment(uint160 accept_commit_hash)
    {
        AcceptCommit accept_commit;
        accept_commit = msgdata[accept_commit_hash]["accept_commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];
        ProceedWithCurrencyPayment(accept_commit, order);
    }

    void TradeHandler::ProceedWithCurrencyPayment(AcceptCommit accept_commit,
                                                  Order order)
    {
        uint160 accept_commit_hash = accept_commit.GetHash160();
        tradedata[accept_commit_hash]["my_sell"] = true;
        if (!order.is_cryptocurrency)
        {
            log_ << "Fiat trade. Not sending until credit is paid\n"
                 << "and ttp acknowledges transaction\n";
            tradedata[accept_commit_hash]["my_fiat_sell"] = true;
            bool credit_paid = tradedata[accept_commit_hash]["credit_paid"];
            bool acknowledged = tradedata[accept_commit_hash]["acknowledged"];
            log_ << "paid: " << credit_paid << "  acknowledged"
                 << acknowledged << "\n";
            if (!(tradedata[accept_commit_hash]["credit_paid"] &&
                  tradedata[accept_commit_hash]["acknowledged"]))
            {
                ScheduleCreditPaymentCheck(accept_commit_hash, 10);
                return;
            }
        }
        log_ << "proceeding with currency payment!!\n";
        vch_t proof = SendCurrency(accept_commit, order);
        if (tradedata[accept_commit_hash]["my_fiat_sell"])
        {
            log_ << "currency sent\n";
            return;
        }
        SendCurrencyPaymentProof(accept_commit_hash, proof);

        OrderCommit commit;
        commit = msgdata[accept_commit.order_commit_hash]["commit"];
        WatchForCreditPayment(accept_commit_hash);
        ScheduleCreditPaymentCheck(accept_commit_hash, 10);
    }

    void TradeHandler::SendDisclosure(uint160 accept_commit_hash,
                                      AcceptCommit& accept_commit,
                                      Order& order,
                                      bool is_response)
    {
        if (is_response)
        {
            uint8_t side = order.side == BID ? ASK : BID;
            std::pair<string_t, uint8_t> disclosure_key;
            disclosure_key = make_pair(string_t("disclosure"), order.side);

            SecretDisclosureMessage earlier_msg; 
            earlier_msg = tradedata[accept_commit_hash][disclosure_key];

            SecretDisclosureMessage msg(
                accept_commit_hash, 
                side, 
                earlier_msg.second_relay_chooser_from_commit);
            BroadcastMessage(msg);
        }
        else
        {
            OrderCommit commit
                = msgdata[accept_commit.order_commit_hash]["commit"];
            uint160 second_relay_chooser_from_commit
                = tradedata[commit.second_relay_chooser_hash]["data"];
            log_ << "SendDisclosure(): order is " << order
                 << "side is " << order.side << "\n";

            SecretDisclosureMessage msg(accept_commit_hash, 
                                        order.side,
                                        second_relay_chooser_from_commit);
            BroadcastMessage(msg);
        }
    }

    void TradeHandler::ScheduleTradeInitiation(uint160 accept_commit_hash)
    {
        teleportnode.scheduler.Schedule("trade_check", accept_commit_hash,
                                    GetTimeMicros() + COMPLAINT_WAIT_TIME);
    }

    void TradeHandler::HandleCommitToOrderIAccepted(OrderCommit order_commit)
    {
        uint160 order_commit_hash = order_commit.GetHash160();
        if (tradedata[order_commit_hash].HasProperty("accept_commit"))
            return;
        log_ << "handling commit to order i accepted\n";
        if (!CheckAndStoreSecretsFromCommit(order_commit))
        {
            log_ << "bad secrets!\n";
            return;
        }
        AcceptCommit accept_commit(order_commit);
        log_ << "broadcasting accept_commit:" << accept_commit;
        accept_commit.Sign();
        uint160 accept_commit_hash = accept_commit.GetHash160();
        tradedata[accept_commit_hash]["is_mine"] = true;
        tradedata[order_commit_hash]["accept_commit"] = accept_commit_hash;
        BroadcastMessage(accept_commit);
    }

    void TradeHandler::HandleAcceptMyOrder(AcceptOrder accept_order)
    {
        log_ << "HandleAcceptMyOrder: " << accept_order;
        if (tradedata[accept_order.order_hash]["cancelled"] ||
            tradedata[accept_order.order_hash]["completed"])
            return;
        
        log_ << "order was not cancelled. committing.\n";
        
        OrderCommit commit(accept_order);
        commit.Sign();
        uint160 order_commit_hash = commit.GetHash160();
        
        tradedata[order_commit_hash]["is_mine"] = true;
        log_ << "Broadcasting commit: " << commit;
        BroadcastMessage(commit);

        log_ << "HandleAcceptMyOrder done\n";
    }


/*
 *  TradeHandler
 *************************/
