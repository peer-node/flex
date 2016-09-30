#ifndef TELEPORT_TRADE_MESSAGES
#define TELEPORT_TRADE_MESSAGES


#include "mining/work.h"
#include "database/data.h"
#include "crypto/teleportcrypto.h"
#include "credits/creditutil.h"
#include "currencies/currencies.h"
#include "teleportnode/wallet.h"
#include "net/net.h"
#include "mining/pit.h"
#include "trade/tradesecret.h"
#include "teleportnode/message_handler.h"

#include "log.h"
#define LOG_CATEGORY "trade_messages.h"

#define ORDER_MEMORY_FACTOR 16  // 0.5 Gb
#define ORDER_NUM_SEGMENTS 64   // 8 Mb
#define ORDER_PROOF_LINKS 32

#define BID 0
#define ASK 1


#define PAYMENT_CONFIRMED 17
#define CANCEL_TRANSACTION 1

#define FORWARD 0
#define BACKWARD 1


std::vector<Point> ChooseRelays(uint160 relay_chooser1, 
                                uint160 relay_chooser2,
                                uint160 credit_hash,
                                uint32_t num_relays = SECRETS_PER_TRADE);

void RememberPrivateKey(uint8_t curve, CBigNum privkey);

CBigNum RandomPrivateKey(uint8_t curve);

bool GetFiatPubKeyAndProof(vch_t currency_code,
                           uint64_t size, 
                           vch_t &fund_proof, 
                           Point &order_pubkey);

bool GetCryptoCurrencyPubKeyAndProof(vch_t currency_code,
                                     uint64_t size,
                                     vch_t &fund_proof,
                                     Point &order_pubkey,
                                     Point &fund_address_pubkey,
                                     Signature &pubkey_authorization);

bool GetCreditPubKeyAndProof(uint64_t amount,
                             Point &order_pubkey,
                             vch_t &fund_proof,
                             Point &fund_address_pubkey);

bool CheckPubKeyAuthorization(vch_t currency_code,
                              Signature pubkey_authorization,
                              Point order_pubkey,
                              Point fund_address_pubkey);

class Order
{
public:
    vch_t currency;
    uint8_t side;
    uint64_t size;
    uint64_t integer_price;
    uint8_t is_cryptocurrency;
    Point order_pubkey;
    Point fund_address_pubkey;
    vch_t fund_proof;
    uint160 relay_chooser_hash;
    TwistWorkProof proof_of_work;
    uint64_t timestamp;
    uint32_t timeout;
    Signature pubkey_authorization;
    Point confirmation_key;
    vch_t auxiliary_data;
    Signature signature;
    
    bool successfully_funded;

    Order() 
    {
        size = 0;
        successfully_funded = false;
    }

    static string_t Type() { return string_t("order"); }

    Order(vch_t currency, uint8_t side,
          uint64_t size,   uint64_t integer_price,
          string_t auxiliary_data);

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n============= Order ===========\n"
           << "== Currency: " 
           << string_t(currency.begin(), currency.end()) << "\n"
           << "== Side: " << (side == BID ? "bid" : "ask") << "\n"
           << "== Size: " << size << "\n"
           << "== Integer Price: " << integer_price << "\n"
           << "== Order Pubkey: " << order_pubkey.ToString() << "\n"
           << "== Fund Address Pubkey: " 
           << fund_address_pubkey.ToString() << "\n"
           << "== Confirmation Key: " << confirmation_key.ToString() << "\n"
           << "== Relay Chooser Hash: "
           << relay_chooser_hash.ToString() << "\n"
           << "== Auxiliary Data: " 
           << string_t(auxiliary_data.begin(), auxiliary_data.end()) << "\n"
           << "== Timestamp: " << timestamp << "\n"
           << "== \n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "=========== End Order =========\n";
        return ss.str();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(currency);
        READWRITE(side);
        READWRITE(size);
        READWRITE(integer_price);
        READWRITE(is_cryptocurrency);
        READWRITE(order_pubkey);
        READWRITE(fund_address_pubkey);
        READWRITE(confirmation_key);
        READWRITE(pubkey_authorization);
        READWRITE(fund_proof);
        READWRITE(relay_chooser_hash);
        READWRITE(proof_of_work);
        READWRITE(auxiliary_data);
        READWRITE(timestamp);
        READWRITE(timeout);
        READWRITE(signature);
    )

    bool operator!()
    {
        return size == 0;
    }

    DEPENDENCIES();

    Point VerificationKey() const
    {
        return order_pubkey;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();

    bool GetFundAddressPubKeyAndProof();

    bool GetFiatPubKeyAndProof()
    {
        return ::GetFiatPubKeyAndProof(currency, size, fund_proof, order_pubkey);
    }

    bool GetCryptoCurrencyPubKeyAndProof()
    {
        return ::GetCryptoCurrencyPubKeyAndProof(currency, 
                                                 size, 
                                                 fund_proof, 
                                                 order_pubkey,
                                                 fund_address_pubkey,
                                                 pubkey_authorization);
    }

    bool CheckPubKeyAuthorization()
    {
        log_ << "CheckPubKeyAuthorization: is crypto: " << is_cryptocurrency
             << "\n";
        if (!is_cryptocurrency)
            return true;
        return ::CheckPubKeyAuthorization(currency,
                                          pubkey_authorization,
                                          order_pubkey,
                                          fund_address_pubkey);
    }

    bool GetCreditPubKeyAndProof()
    {
        uint64_t amount = size * integer_price;
        return ::GetCreditPubKeyAndProof(amount, 
                                         order_pubkey, 
                                         fund_proof, 
                                         fund_address_pubkey);
    }
};


class AcceptOrder
{
public:
    vch_t currency;
    uint160 order_hash;
    uint8_t accept_side;
    Point order_pubkey;
    Point fund_address_pubkey;
    Signature pubkey_authorization;
    vch_t fund_proof;
    uint160 relay_chooser;
    uint160 previous_credit_hash;
    TwistWorkProof proof_of_work;
    vch_t auxiliary_data;
    Signature signature;

    bool successfully_funded;
    uint64_t amount;

    AcceptOrder() {}

    AcceptOrder(Order order, string_t auxiliary_data);

    static string_t Type() { return string_t("accept_order"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n============= AcceptOrder ===========\n"
           << "== Currency: " 
           << string_t(currency.begin(), currency.end()) << "\n"
           << "== Order Hash: " << order_hash.ToString() << "\n"
           << "== Accept Side: " 
           << (accept_side == BID ? "bid" : "ask") << "\n"
           << "== Accept Order Pubkey: " << order_pubkey.ToString() << "\n"
           << "== Fund Address Pubkey: " 
           << fund_address_pubkey.ToString() << "\n"
           << "== Relay Chooser: " << relay_chooser.ToString() << "\n"
           << "== Previous Credit Hash: " 
           << previous_credit_hash.ToString() << "\n"
           << "== Auxiliary Data: " 
           << string_t(auxiliary_data.begin(), auxiliary_data.end()) << "\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "=========== End AcceptOrder =========\n";
        return ss.str();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(currency);
        READWRITE(order_hash);
        READWRITE(accept_side);
        READWRITE(order_pubkey);
        READWRITE(fund_address_pubkey);
        READWRITE(previous_credit_hash);
        READWRITE(fund_proof);
        READWRITE(relay_chooser);
        READWRITE(proof_of_work);
        READWRITE(auxiliary_data);
        READWRITE(signature);
    )

    DEPENDENCIES(order_hash, previous_credit_hash);

    Point VerificationKey() const
    {
        return order_pubkey;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();


    bool GetFundAddressPubKeyAndProof();

    bool GetFiatPubKeyAndProof()
    {
        return ::GetFiatPubKeyAndProof(currency, 
                                       amount, 
                                       fund_proof, 
                                       order_pubkey);
    }

    bool GetCryptoCurrencyPubKeyAndProof()
    {
        return ::GetCryptoCurrencyPubKeyAndProof(currency, 
                                                 amount, 
                                                 fund_proof, 
                                                 order_pubkey,
                                                 fund_address_pubkey,
                                                 pubkey_authorization);
    }

    bool CheckPubKeyAuthorization()
    {
        return ::CheckPubKeyAuthorization(currency,
                                          pubkey_authorization,
                                          order_pubkey,
                                          fund_address_pubkey);
    }

    bool GetCreditPubKeyAndProof()
    {
        return ::GetCreditPubKeyAndProof(amount, 
                                         order_pubkey, 
                                         fund_proof, 
                                         fund_address_pubkey);
    }
};

class CancelOrder
{
public:
    uint160 order_hash;
    Signature signature;

    CancelOrder() { }

    CancelOrder(uint160 order_hash):
        order_hash(order_hash)
    { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(order_hash);
        READWRITE(signature);
    )

    static string_t Type() { return string_t("cancel"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n============= CancelOrder ===========\n"
           << "== Order Hash: " << order_hash.ToString() << "\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "=========== End CancelOrder =========\n";
        return ss.str();
    }

    DEPENDENCIES(order_hash);

    Point VerificationKey() const
    {
        Order order = msgdata[order_hash]["order"];
        return order.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class TraderLeaveMessage
{
public:
    std::vector<uint160> order_hashes;
    Signature signature;

    TraderLeaveMessage();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(order_hashes);
        READWRITE(signature);
    )

    static string_t Type() { return string_t("trader_leave"); }

    std::vector<uint160> Dependencies()
    {
        return order_hashes;
    }

    Point VerificationKey()
    {
        Point verification_key(SECP256K1, 0);
        CBigNum privkey_if_mine(0);
        foreach_(uint160 order_hash, order_hashes)
        {
            Order order = msgdata[order_hash]["order"];
            Point order_verification_key = order.VerificationKey();
            verification_key += order_verification_key;
            if (keydata[order_verification_key].HasProperty("privkey"))
            {
                CBigNum privkey = keydata[order_verification_key]["privkey"];
                privkey_if_mine += privkey;
                privkey_if_mine = privkey_if_mine % verification_key.Modulus();
            }
        }
        if (privkey_if_mine != 0)
            keydata[verification_key]["privkey"] = privkey_if_mine;
        return verification_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class OrderCommit
{
public:
    uint8_t curve;
    uint160 order_hash;
    uint160 accept_order_hash;
    uint160 relay_chooser;
    uint160 second_relay_chooser_hash;
    DistributedTradeSecret distributed_trade_secret;
    CBigNum shared_credit_secret_xor_shared_secret;
    Point shared_credit_pubkey;
    CBigNum shared_currency_secret_xor_hash_of_shared_credit_secret;
    Point shared_currency_pubkey;
    Signature signature;

    OrderCommit() {}

    static string_t Type() { return string_t("commit"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n============= OrderCommit ===========\n"
           << "== Order Hash: " << order_hash.ToString() << "\n"
           << "== Accept Order Hash: " 
           << accept_order_hash.ToString() << "\n"
           << "== Relay Chooser: " << relay_chooser.ToString() << "\n"
           << "== Second Relay Chooser Hash: " 
           << second_relay_chooser_hash.ToString() << "\n"
           << "== Shared Credit Point: "
           << shared_credit_pubkey.ToString() << "\n"
           << "== Shared Credit Secret ^ Shared Secret: "
           << shared_credit_secret_xor_shared_secret.ToString() << "\n"
           << "== Shared Currency Point: "
           << shared_currency_pubkey.ToString() << "\n"
           << "== Shared Currency Secret ^ Hash Shared Credit Secret: "
           << shared_currency_secret_xor_hash_of_shared_credit_secret.ToString()
           << "==\n"
           << "== Distributed Trade Secret:" 
           << distributed_trade_secret.ToString()
           << "\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "=========== End OrderCommit =========\n";
        return ss.str();
    }

    OrderCommit(AcceptOrder accept_order);

    void GenerateSecrets(AcceptOrder accept_order)
    {
        CBigNum shared_credit_secret = RandomPrivateKey(SECP256K1);
        RememberPrivateKey(SECP256K1, shared_credit_secret);

        shared_credit_pubkey = Point(SECP256K1, shared_credit_secret);
        log_ << "shared credit secret = "
             << shared_credit_secret << " and pubkey = "
             << shared_credit_pubkey << "\n";
        
        CBigNum shared_currency_secret = RandomPrivateKey(curve);
        RememberPrivateKey(curve, shared_currency_secret);
        shared_currency_pubkey = Point(SECP256K1, shared_currency_secret);
    }

    void EncryptSecrets(AcceptOrder accept_order)
    {
        CBigNum shared_credit_secret, shared_currency_secret, shared_secret;
        
        shared_credit_secret = keydata[shared_credit_pubkey]["privkey"];
        shared_currency_secret = keydata[shared_currency_pubkey]["privkey"];

        AcceptOrder order = msgdata[accept_order_hash]["accept_order"];
        Point counterparty = accept_order.order_pubkey;
        shared_secret = Hash(shared_credit_secret * counterparty);

        shared_credit_secret_xor_shared_secret
            = shared_credit_secret ^ shared_secret;
        
        shared_currency_secret_xor_hash_of_shared_credit_secret
            = shared_currency_secret ^ Hash(shared_credit_secret);
    }

    DEPENDENCIES(order_hash, accept_order_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(curve);
        READWRITE(order_hash);
        READWRITE(accept_order_hash);
        READWRITE(relay_chooser);
        READWRITE(second_relay_chooser_hash);
        READWRITE(distributed_trade_secret);
        READWRITE(shared_credit_secret_xor_shared_secret);
        READWRITE(shared_credit_pubkey);
        READWRITE(shared_currency_secret_xor_hash_of_shared_credit_secret);
        READWRITE(shared_currency_pubkey);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        Order order = msgdata[order_hash]["order"];
        return order.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class AcceptCommit
{
public:
    uint160 order_hash;
    uint160 order_commit_hash;
    uint160 second_relay_chooser;
    uint8_t curve;
    DistributedTradeSecret distributed_trade_secret;
    Signature signature;

    AcceptCommit() {}

    static string_t Type() { return string_t("accept_commit"); }

    AcceptCommit(OrderCommit commit):
        order_hash(commit.order_hash),
        order_commit_hash(commit.GetHash160())
    {
        printf("accept_commit: accepting commit with hash %s\n",
            commit.GetHash160().ToString().c_str());
        uint160 accept_order_hash = commit.accept_order_hash;
        AcceptOrder accept = msgdata[accept_order_hash]["accept_order"];
        uint160 relay_chooser = accept.relay_chooser ^ commit.relay_chooser;
        curve = commit.curve;
        distributed_trade_secret
            = DistributedTradeSecret(curve,
                                     accept.previous_credit_hash,
                                     relay_chooser);
        second_relay_chooser = Rand160();
    }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n============= AcceptCommit ===========\n"
           << "== Order Hash: " << order_hash.ToString() << "\n"
           << "== Order Commit Hash: " 
           << order_commit_hash.ToString() << "\n"
           << "== Second Relay Chooser: " 
           << second_relay_chooser.ToString() << "\n"
           << "==\n"
           << "== Distributed Trade Secret:" 
           << distributed_trade_secret.ToString()
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "=========== End AcceptCommit =========\n";
        return ss.str();
    }

    DEPENDENCIES(order_hash, order_commit_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(curve);
        READWRITE(order_hash);
        READWRITE(order_commit_hash);
        READWRITE(second_relay_chooser);
        READWRITE(distributed_trade_secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        printf("acceptcommit::VerificationKey()\n");
        printf("retrieving commit with hash %s\n",
            order_commit_hash.ToString().c_str());
        OrderCommit commit = msgdata[order_commit_hash]["commit"];
        uint160 accept_order_hash = commit.accept_order_hash;
        printf("retrieving accept with hash %s\n",
            accept_order_hash.ToString().c_str());
        AcceptOrder accept = msgdata[accept_order_hash]["accept_order"];
        printf("accept says order hash is %s\n",
            accept.order_hash.ToString().c_str());
        printf("returning pubkey: %s\n",
            accept.VerificationKey().ToString().c_str());
        return accept.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class SecretDisclosureMessage
{
public:
    uint160 accept_commit_hash;
    uint160 second_relay_chooser_from_commit;
    uint160 second_relay_chooser_from_accept_commit;
    bool original_secret_in_accept_commit;
    uint8_t side;
    DistributedTradeSecretDisclosure disclosure;
    Signature signature;

    SecretDisclosureMessage() { }

    SecretDisclosureMessage(uint160 accept_commit_hash, 
                            uint8_t side,
                            uint160 second_relay_chooser_from_commit):
        accept_commit_hash(accept_commit_hash),
        second_relay_chooser_from_commit(second_relay_chooser_from_commit),
        side(side)
    {
        AcceptCommit accept_commit;
        accept_commit = msgdata[accept_commit_hash]["accept_commit"];
        log_ << "SecretDisclosureMessage(): accept commit is " 
             << accept_commit;

        second_relay_chooser_from_accept_commit
            = accept_commit.second_relay_chooser;
        OrderCommit commit;
        commit = msgdata[accept_commit.order_commit_hash]["commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];

        uint160 relay_chooser = second_relay_chooser_from_accept_commit 
                                 ^ second_relay_chooser_from_commit;

        if (side == order.side)
        {
            log_ << "side " << side << " == " << order.side << "\n"
                 << "getting distributed secret from commit" << "\n";
            original_secret_in_accept_commit = true;
            disclosure = DistributedTradeSecretDisclosure(
                                            commit.distributed_trade_secret,
                                            relay_chooser);
        }
        else
        {
            log_ << "side " << side << " != " << order.side << "\n"
                 << "getting distributed secret from accept_commit" << "\n";
            original_secret_in_accept_commit = false;
            disclosure = DistributedTradeSecretDisclosure(
                                    accept_commit.distributed_trade_secret,
                                    relay_chooser);
        }

        log_ << "SecretDisclosureMessage created: " << *this;
    }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n============= SecretDisclosureMessage ===========\n"
           << "== Accept Commit Hash: " 
           << accept_commit_hash.ToString() << "\n"
           << "== 2nd Relay Chooser from Commit: " 
           << second_relay_chooser_from_commit.ToString() << "\n"
           << "== 2nd Relay Chooser from Accept Commit: " 
           << second_relay_chooser_from_accept_commit.ToString() << "\n"
           << "== Side: " << (side == BID ? "bid" : "ask") << "\n"
           << "== Original Secret in: "
           << (original_secret_in_accept_commit ? "Accept Commit": "Commit")
           << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "=========== End SecretDisclosureMessage =========\n";
        return ss.str();
    }

    bool CheckRelays()
    {
        AcceptCommit accept_commit;
        accept_commit = msgdata[accept_commit_hash]["accept_commit"];
        
        OrderCommit commit;
        commit = msgdata[accept_commit.order_commit_hash]["commit"];
        

        if(second_relay_chooser_from_accept_commit
            != accept_commit.second_relay_chooser ||
           HashUint160(second_relay_chooser_from_commit)
            != commit.second_relay_chooser_hash)
            return false;

        return (disclosure.relay_chooser
                    == (second_relay_chooser_from_accept_commit 
                         ^ second_relay_chooser_from_commit))
                && disclosure.Validate();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(accept_commit_hash);
        READWRITE(side);
        READWRITE(disclosure);
        READWRITE(original_secret_in_accept_commit);
        READWRITE(second_relay_chooser_from_commit);
        READWRITE(second_relay_chooser_from_accept_commit);
        READWRITE(signature);
    )

    static string_t Type() { return string_t("disclosure"); }

    DEPENDENCIES(accept_commit_hash);

    Point VerificationKey() const
    {
        AcceptCommit accept_commit
            = msgdata[accept_commit_hash]["accept_commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];

        if (order.side == side)
            return order.VerificationKey();
        return accept_commit.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class CurrencyPaymentProof
{
public:
    uint160 accept_commit_hash;
    vch_t proof;
    Signature signature;

    CurrencyPaymentProof() {}

    static string_t Type() { return string_t("payment_proof"); }

    CurrencyPaymentProof(uint160 accept_commit_hash, vch_t proof):
        accept_commit_hash(accept_commit_hash),
        proof(proof)
    { 
        printf("CurrencyPaymentProof(): accept commit hash=%s\n", 
            accept_commit_hash.ToString().c_str());
    }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n============= CurrencyPaymentProof ===========\n"
           << "== Accept Commit Hash: " 
           << accept_commit_hash.ToString() << "\n"
           << "== Proof: " << string_t(proof.begin(), proof.end()) << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "=========== End CurrencyPaymentProof =========\n";
        return ss.str();
    }

    DEPENDENCIES(accept_commit_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(accept_commit_hash);
        READWRITE(proof);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        printf("getting verification key for payment proof\n");
        printf("accept commit hash is %s\n",
                accept_commit_hash.ToString().c_str());
        AcceptCommit accept_commit
            = msgdata[accept_commit_hash]["accept_commit"];
        printf("order commit hash is %s\n",
            accept_commit.order_commit_hash.ToString().c_str());
        OrderCommit commit
            = msgdata[accept_commit.order_commit_hash]["commit"];
        printf("accept order hash is %s\n",
            commit.accept_order_hash.ToString().c_str());
        AcceptOrder accept
            = msgdata[commit.accept_order_hash]["accept_order"];
        printf("order hash is %s\n", 
            accept.order_hash.ToString().c_str());
        Order order = msgdata[accept.order_hash]["order"];
        if (order.side == BID)
            return accept.VerificationKey();
        else
            return order.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

void SendCurrencyPaymentProof(uint160 accept_commit_hash, vch_t proof);

bool CheckCurrencyPaymentProof(CurrencyPaymentProof payment_proof);

void HandleCurrencyPaymentProof(CurrencyPaymentProof payment_proof,
                                bool scheduled=false);

class ThirdPartyTransactionAcknowledgement
{
public:
    uint160 order_hash;
    uint160 accept_commit_hash;
    Signature signature;

    ThirdPartyTransactionAcknowledgement() { }

    static string_t Type() { return string_t("acknowledgement"); }

    ThirdPartyTransactionAcknowledgement(uint160 accept_commit_hash):
        accept_commit_hash(accept_commit_hash)
    {
        AcceptCommit accept_commit
            = msgdata[accept_commit_hash]["accept_commit"];
        order_hash = accept_commit.order_hash;
    }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== ThirdPartyTransactionAcknowledgement =========\n"
           << "== Order Hash: " << order_hash.ToString() << "\n"
           << "== Accept Commit Hash: " 
           << accept_commit_hash.ToString() << "\n"
           << "== Third Party Key: " << VerificationKey().ToString() << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End ThirdPartyTransactionAcknowledgement =======\n";
        return ss.str();
    }

    DEPENDENCIES(order_hash, accept_commit_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(order_hash);
        READWRITE(accept_commit_hash);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        Order order = msgdata[order_hash]["order"];
        return order.confirmation_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class ThirdPartyPaymentConfirmation
{
public:
    uint160 order_hash;
    uint160 accept_commit_hash;
    uint160 acknowledgement_hash;
    uint8_t affirmation;
    Signature signature;

    ThirdPartyPaymentConfirmation() { }

    static string_t Type() { return string_t("third_party_confirmation"); }

    ThirdPartyPaymentConfirmation(uint160 acknowledgement_hash):
        acknowledgement_hash(acknowledgement_hash),
        affirmation(PAYMENT_CONFIRMED)
    {
        ThirdPartyTransactionAcknowledgement acknowledgement
            = msgdata[acknowledgement_hash]["acknowledgement"];
        accept_commit_hash = acknowledgement.accept_commit_hash;
        AcceptCommit accept_commit
            = msgdata[accept_commit_hash]["accept_commit"];
        order_hash = accept_commit.order_hash;
    }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== ThirdPartyPaymentConfirmation =========\n"
           << "== Order Hash: " << order_hash.ToString() << "\n"
           << "== Accept Commit Hash: " 
           << accept_commit_hash.ToString() << "\n"
           << "== Acknowledgement Hash: " 
           << acknowledgement_hash.ToString() << "\n"
           << "== Third Party Key: " << VerificationKey().ToString() << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End ThirdPartyPaymentConfirmation =======\n";
        return ss.str();
    }

    DEPENDENCIES(order_hash, accept_commit_hash, acknowledgement_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(affirmation);
        READWRITE(order_hash);
        READWRITE(accept_commit_hash);
        READWRITE(acknowledgement_hash);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        AcceptCommit accept_commit
            = msgdata[accept_commit_hash]["accept_commit"];
        if (accept_commit.order_hash != order_hash)
            return Point(SECP256K1, 0);
        Order order = msgdata[order_hash]["order"];
        return order.confirmation_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class CurrencyPaymentConfirmation
{
public:
    uint160 order_hash;
    uint160 accept_commit_hash;
    uint160 payment_hash;
    char affirmation;
    Signature signature;

    CurrencyPaymentConfirmation() { }

    static string_t Type() { return string_t("confirmation"); }

    CurrencyPaymentConfirmation(uint160 accept_commit_hash,
                                uint160 payment_hash):
        accept_commit_hash(accept_commit_hash),
        payment_hash(payment_hash),
        affirmation(PAYMENT_CONFIRMED)
    {
        AcceptCommit accept_commit
            = msgdata[accept_commit_hash]["accept_commit"];
        order_hash = accept_commit.order_hash;
    }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== CurrencyPaymentConfirmation =========\n"
           << "== Order Hash: " << order_hash.ToString() << "\n"
           << "== Accept Commit Hash: " 
           << accept_commit_hash.ToString() << "\n"
           << "== Payment Hash: " << payment_hash.ToString() << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End CurrencyPaymentConfirmation =======\n";
        return ss.str();
    }

    DEPENDENCIES(order_hash, accept_commit_hash, payment_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(affirmation);
        READWRITE(order_hash);
        READWRITE(accept_commit_hash);
        READWRITE(payment_hash);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        Order order = msgdata[order_hash]["order"];
        AcceptCommit accept_commit
            = msgdata[accept_commit_hash]["accept_commit"];
        if (accept_commit.order_hash != order_hash)
            return Point(SECP256K1, 0);
        if (order.side == BID)
            return order.VerificationKey();
        else
        {
            OrderCommit order_commit
                = msgdata[accept_commit.order_commit_hash]["commit"];
            AcceptOrder accept
                = msgdata[order_commit.accept_order_hash]["accept_order"];
            return accept.VerificationKey();
        }
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class TradeSecretMessage
{
public:
    uint160 accept_commit_hash;
    uint8_t direction;
    uint32_t position;
    CBigNum credit_secret_xor_shared_secret;
    CBigNum currency_secret_xor_other_shared_secret;
    Signature signature;

    TradeSecretMessage() { }

    static string_t Type() { return string_t("secret"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== TradeSecretMessage =========\n"
           << "== Accept Commit Hash: " 
           << accept_commit_hash.ToString() << "\n"
           << "== Direction: " << (bool)direction << "\n"
           << "== Position: " << position << "\n"
           << "==\n"
           << "== Credit Secret xor Shared Secret: " 
           << credit_secret_xor_shared_secret.ToString() << "\n"
           << "== Currency Secret xor Other Shared Secret: "
           << currency_secret_xor_other_shared_secret.ToString() << "\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End TradeSecretMessage =======\n";
        return ss.str();
    }

    TradeSecretMessage(uint160 accept_commit_hash,
                       uint8_t direction,
                       CBigNum credit_secret_xor_shared_secret,
                       CBigNum currency_secret_xor_other_shared_secret,
                       uint32_t position):
        accept_commit_hash(accept_commit_hash),
        direction(direction),
        position(position),
        credit_secret_xor_shared_secret(credit_secret_xor_shared_secret),
        currency_secret_xor_other_shared_secret(
                    currency_secret_xor_other_shared_secret)
    { }

    DEPENDENCIES(accept_commit_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(accept_commit_hash);
        READWRITE(direction);
        READWRITE(position);
        READWRITE(credit_secret_xor_shared_secret);
        READWRITE(currency_secret_xor_other_shared_secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        AcceptCommit accept_commit;
        accept_commit = msgdata[accept_commit_hash]["accept_commit"];
        OrderCommit commit;
        commit = msgdata[accept_commit.order_commit_hash]["commit"];
        DistributedTradeSecret secret = commit.distributed_trade_secret;
        Point relay_key = secret.Relays()[position];
        
        return relay_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class BackupTradeSecretMessage
{
public:
    uint160 accept_commit_hash;
    uint8_t direction;
    uint32_t position;
    CBigNum credit_secret_xor_shared_secret;
    CBigNum currency_secret_xor_other_shared_secret;
    Signature signature;

    BackupTradeSecretMessage() { }

    static string_t Type() { return string_t("backup_secret"); }

    BackupTradeSecretMessage(uint160 accept_commit_hash,
                       uint8_t direction,
                       CBigNum credit_secret_xor_shared_secret,
                       CBigNum currency_secret_xor_other_shared_secret,
                       uint32_t position):
        accept_commit_hash(accept_commit_hash),
        direction(direction),
        position(position),
        credit_secret_xor_shared_secret(credit_secret_xor_shared_secret),
        currency_secret_xor_other_shared_secret(
                    currency_secret_xor_other_shared_secret)
    { }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== BackupTradeSecretMessage =========\n"
           << "== Accept Commit Hash: " 
           << accept_commit_hash.ToString() << "\n"
           << "== Direction: " << direction << "\n"
           << "== Position: " << position << "\n"
           << "== Credit Secret ^ Shared Secret: "
           << credit_secret_xor_shared_secret.ToString() << "\n"
           << "== Currency Secret ^ Other Shared Secret: "
           << currency_secret_xor_other_shared_secret.ToString() << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End BackupTradeSecretMessage =======\n";
        return ss.str();
    }

    DEPENDENCIES(accept_commit_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(accept_commit_hash);
        READWRITE(direction);
        READWRITE(position);
        READWRITE(credit_secret_xor_shared_secret);
        READWRITE(currency_secret_xor_other_shared_secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        std::pair<string_t, uint8_t> key;
        key = make_pair(string_t("disclosure"), BID);
        SecretDisclosureMessage msg = tradedata[accept_commit_hash][key];
        return msg.disclosure.Relays()[position];
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class CounterpartySecretMessage
{
public:
    uint160 accept_commit_hash;
    uint8_t sender_side;
    CBigNum secret_xor_shared_secret;
    Signature signature;

    CounterpartySecretMessage() { }

    static string_t Type() { return string_t("counterparty_secret"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== CounterpartySecretMessage =========\n"
           << "== Accept Commit Hash: " 
           << accept_commit_hash.ToString() << "\n"
           << "== Sender Side: " 
           << (sender_side == BID ? "bid" : "ask") << "\n"
           << "== Secret ^ Shared Secret: "
           << secret_xor_shared_secret.ToString() << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End CounterpartySecretMessage =======\n";
        return ss.str();
    }

    CounterpartySecretMessage(uint160 accept_commit_hash,
                              uint8_t sender_side):
        accept_commit_hash(accept_commit_hash),
        sender_side(sender_side)
    {
        log_ << "CounterpartySecretMessage(): " << accept_commit_hash
             << "\n";
        AcceptCommit accept_commit;
        accept_commit = msgdata[accept_commit_hash]["accept_commit"];
        OrderCommit commit;
        commit = msgdata[accept_commit.order_commit_hash]["commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];

        CBigNum privkey;
        Point pubkey, counterparty_key;

        DistributedTradeSecret secret;

        if (sender_side == order.side)
        {
            secret = commit.distributed_trade_secret;
        }
        else
        {
            secret = accept_commit.distributed_trade_secret;
        }

        log_ << "relevant secret is " << secret;

        pubkey = (sender_side == BID) ? secret.CreditPubKey() 
                                      : secret.CurrencyPubKey();

        log_ << "pubkey is " << pubkey << "\n";

        privkey = keydata[pubkey]["privkey"];
#ifndef _PRODUCTION_BUILD
        log_ << "privkey is " << privkey << "\n";
#endif

        if (sender_side == order.side)
            counterparty_key = accept_commit.VerificationKey();
        else
            counterparty_key = commit.VerificationKey();

        log_ << "counterparty_key is " << counterparty_key << "\n";

        CBigNum shared_secret = Hash(privkey * counterparty_key);

        log_ << "shared secret is " << shared_secret << "\n";
        secret_xor_shared_secret = privkey ^ shared_secret;
        log_ << "secret_xor_shared_secret = " 
             << secret_xor_shared_secret << "\n";
    }

    DEPENDENCIES(accept_commit_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(accept_commit_hash);
        READWRITE(sender_side);
        READWRITE(secret_xor_shared_secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        AcceptCommit accept_commit;
        accept_commit = msgdata[accept_commit_hash]["accept_commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];
        if (order.side == sender_side)
            return order.VerificationKey();
        else
            return accept_commit.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class CounterpartySecretComplaint
{
public:
    uint160 secret_message_hash;
    uint8_t sender_side;
    Signature signature;

    CounterpartySecretComplaint() { }

    static string_t Type() { return string_t("counterparty_complaint"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== CounterpartySecretComplaint =========\n"
           << "== Secret Message Hash: " 
           << secret_message_hash.ToString() << "\n"
           << "== Sender Side: " 
           << (sender_side == BID ? "bid" : "ask") << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End CounterpartySecretComplaint =======\n";
        return ss.str();
    }

    CounterpartySecretComplaint(uint160 secret_message_hash, 
                                uint8_t sender_side):
        secret_message_hash(secret_message_hash),
        sender_side(sender_side)
    { }

    DEPENDENCIES(secret_message_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(secret_message_hash);
        READWRITE(sender_side);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        CounterpartySecretMessage msg
            = msgdata[secret_message_hash]["counterparty_secret"];
        AcceptCommit accept_commit;
        accept_commit = msgdata[msg.accept_commit_hash]["accept_commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];
        if (order.side == sender_side)
            return accept_commit.VerificationKey();
        else
            return order.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class RelayComplaint
{
public:
    uint160 message_hash;
    uint8_t bad_secret_side;
    uint32_t position;
    CBigNum bad_secret;
    Signature signature;

    RelayComplaint() { }

    static string_t Type() { return string_t("relay_complaint"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== RelayComplaint =========\n"
           << "== Message Hash: " 
           << message_hash.ToString() << "\n"
           << "== Bad Secret Side: " 
           << (bad_secret_side == BID ? "bid" : "ask") << "\n"
           << "== Bad Secret: " << bad_secret.ToString() << "\n"
           << "== Position: " << position << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End RelayComplaint =======\n";
        return ss.str();
    }

    RelayComplaint(uint160 message_hash,
                   bool bad_secret_side,
                   uint32_t position,
                   CBigNum bad_shared_secret):
        message_hash(message_hash),
        bad_secret_side(bad_secret_side),
        position(position),
        bad_secret(bad_shared_secret)
    { }

    DEPENDENCIES(message_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(message_hash);
        READWRITE(bad_secret_side);
        READWRITE(position);
        READWRITE(bad_secret);
        READWRITE(signature);
    )

    DistributedTradeSecret GetDistributedTradeSecret()
    {
        string_t type = msgdata[message_hash]["type"];
        if (type == "commit")
        {
            OrderCommit commit = msgdata[message_hash]["commit"];
            return commit.distributed_trade_secret;
        }
        if (type == "accept_commit")
        {
            AcceptCommit accept_commit
                = msgdata[message_hash]["accept_commit"];
            return accept_commit.distributed_trade_secret;
        }
        return DistributedTradeSecret();
    }

    bool Validate()
    {
        string_t type = msgdata[message_hash]["type"];
        if (type == "disclosure")
        {
            SecretDisclosureMessage msg
                = msgdata[message_hash]["disclosure"];
                
            return ValidateGivenDisclosureMessage(msg);
        }
        DistributedTradeSecret secret = GetDistributedTradeSecret();
        return ValidateGivenDistSecret(secret);
    }

    bool ValidateGivenDistSecret(DistributedTradeSecret secret)
    {
        PieceOfSecret piece = secret.pieces[position];
        if (bad_secret_side == BID)
        {
            return piece.credit_pubkey != 
                    Point(SECP256K1, bad_secret ^
                          piece.credit_secret_xor_shared_secret);
        }
        else
        {
            return (piece.credit_pubkey == Point(SECP256K1, bad_secret) &&
                    piece.currency_pubkey != 
                    Point(piece.currency_pubkey.curve,
                          Hash(bad_secret) ^
                          piece.currency_secret_xor_hash_of_credit_secret));
        }
    }

    bool ValidateGivenDisclosureMessage(SecretDisclosureMessage msg)
    {
        AcceptCommit accept_commit
            = msgdata[msg.accept_commit_hash]["accept_commit"];
        OrderCommit commit
            = msgdata[accept_commit.order_commit_hash]["commit"];

        RevelationOfPieceOfSecret revelation
            = msg.disclosure.revelations[position];

        Point recipient;
        DistributedTradeSecret secret;

        if (msg.VerificationKey() == accept_commit.VerificationKey())
        {
            recipient = commit.VerificationKey();
            secret = accept_commit.distributed_trade_secret;
        }
        else if (msg.VerificationKey() == commit.VerificationKey())
        {
            recipient = accept_commit.VerificationKey();
            secret = commit.distributed_trade_secret;
        }
        else return false;

        PieceOfSecret piece = secret.pieces[position];

        if (bad_secret_side == BID) // shared secret bad
        {
            if (piece.credit_pubkey == 
                    Point(SECP256K1, bad_secret ^
                          revelation.credit_secret_xor_second_shared_secret))
            {
                if (piece.credit_secret_xor_shared_secret 
                    == (bad_secret ^ Hash(recipient * bad_secret)))
                    return false;
            }
            else
                return true;
        }
        // credit secret good; currency secret bad
        
        return (piece.credit_pubkey == Point(SECP256K1, bad_secret) &&
                piece.currency_pubkey != 
                Point(piece.currency_pubkey.curve,
                      Hash(bad_secret) ^
                      piece.currency_secret_xor_hash_of_credit_secret));
    
    }

    Point VerificationKey()
    {
        string_t type = msgdata[message_hash]["type"];
        if (type == "disclosure")
        {
            SecretDisclosureMessage msg = msgdata[message_hash]["disclosure"];
            return msg.disclosure.Relays()[position];
        }
        
        return GetDistributedTradeSecret().Relays()[position];
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class RefutationOfRelayComplaint
{
public:
    uint160 complaint_hash;
    CBigNum good_credit_secret;
    Signature signature;

    RefutationOfRelayComplaint() { }

    RefutationOfRelayComplaint(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    {
        good_credit_secret = GetGoodCreditSecret();
    }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== RefutationOfRelayComplaint =========\n"
           << "== Complaint Hash: " << complaint_hash.ToString() << "\n"
           << "== Good Credit Secret: " 
           << good_credit_secret.ToString() << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End RefutationOfRelayComplaint =======\n";
        return ss.str();
    }

    Point VerificationKey() const
    {
        RelayComplaint complaint
            = msgdata[complaint_hash]["relay_complaint"];

        uint160 hash = complaint.message_hash;
        string_t type = msgdata[hash]["type"];
        if (type == "disclosure")
        {
            SecretDisclosureMessage msg = msgdata[hash]["disclosure"];
            return msg.VerificationKey();
        }
        else if (type == "commit")
        {
            OrderCommit commit = msgdata[hash]["commit"];
            return commit.VerificationKey();
        }
        else if (type == "accept_commit")
        {
            AcceptCommit accept_commit = msgdata[hash]["acccept_commit"];
            return accept_commit.VerificationKey();
        }
        return Point(SECP256K1, 0);
    }

    bool Validate()
    {
        RelayComplaint complaint
            = msgdata[complaint_hash]["relay_complaint"];
        string_t type = msgdata[complaint.message_hash]["type"];
        if (type == "disclosure")
            return VerifyGoodCreditSecretInDisclosure(complaint);
        else if (type == "commit")
            return VerifyGoodCreditSecretInCommit(complaint);
        else if (type == "accept_commit")
            return VerifyGoodCreditSecretInAcceptCommit(complaint);
        return false;
    }

    bool VerifyGoodCreditSecretInCommit(RelayComplaint complaint)
    {
        OrderCommit commit = msgdata[complaint.message_hash]["commit"];
        DistributedTradeSecret secret = commit.distributed_trade_secret;
        PieceOfSecret piece = secret.pieces[complaint.position];
        Point pubkey = piece.credit_pubkey;
        return pubkey == Point(pubkey.curve, good_credit_secret);
    }

    bool VerifyGoodCreditSecretInAcceptCommit(RelayComplaint complaint)
    {
        AcceptCommit accept_commit
            = msgdata[complaint.message_hash]["accept_commit"];
        DistributedTradeSecret secret = accept_commit.distributed_trade_secret;
        PieceOfSecret piece = secret.pieces[complaint.position];
        Point pubkey = piece.credit_pubkey;
        return pubkey == Point(pubkey.curve, good_credit_secret);
    }

    DistributedTradeSecret GetSecretViaDisclosure()
    {
        RelayComplaint complaint
            = msgdata[complaint_hash]["relay_complaint"];
        SecretDisclosureMessage msg
            = msgdata[complaint.message_hash]["disclosure"];
        AcceptCommit accept_commit
            = msgdata[msg.accept_commit_hash]["accept_commit"];
        OrderCommit commit
            = msgdata[accept_commit.order_commit_hash]["commit"];

        DistributedTradeSecret secret;
        if (msg.original_secret_in_accept_commit)
            secret = accept_commit.distributed_trade_secret;
        else
            secret = commit.distributed_trade_secret;
        return secret;
    }

    bool VerifyGoodCreditSecretInDisclosure(RelayComplaint complaint)
    {
        DistributedTradeSecret secret = GetSecretViaDisclosure();
        return Point(SECP256K1, good_credit_secret)
                == secret.pieces[complaint.position].credit_pubkey;
    }

    CBigNum GetGoodCreditSecret()
    {
        RelayComplaint complaint
            = msgdata[complaint_hash]["relay_complaint"];
        Point pubkey = GetCreditPubKey(complaint);
        CBigNum credit_secret = keydata[pubkey]["privkey"];
        return credit_secret;
    }

    Point GetCreditPubKey(RelayComplaint complaint)
    {
        string_t type = msgdata[complaint.message_hash]["type"];
        if (type == "disclosure")
            return GetCreditPubKeyFromDisclosure(complaint);
        else if (type == "commit")
            return GetCreditPubKeyFromCommit(complaint);
        else if (type == "accept_commit")
            return GetCreditPubKeyFromAcceptCommit(complaint);
        return Point(SECP256K1, 0);
    }

    Point GetCreditPubKeyFromDisclosure(RelayComplaint complaint)
    {
        DistributedTradeSecret secret = GetSecretViaDisclosure();
        return secret.pieces[complaint.position].credit_pubkey;
    }

    Point GetCreditPubKeyFromCommit(RelayComplaint complaint)
    {
        OrderCommit commit = msgdata[complaint.message_hash]["commit"];
        DistributedTradeSecret secret = commit.distributed_trade_secret;
        PieceOfSecret piece = secret.pieces[complaint.position];
        return piece.credit_pubkey;
    }

    Point GetCreditPubKeyFromAcceptCommit(RelayComplaint complaint)
    {
        AcceptCommit accept_commit
            = msgdata[complaint.message_hash]["accept_commit"];
        DistributedTradeSecret secret = accept_commit.distributed_trade_secret;
        PieceOfSecret piece = secret.pieces[complaint.position];
        return piece.credit_pubkey;
    }

    static string_t Type() { return string_t("relay_complaint_refutation"); }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(good_credit_secret);
        READWRITE(signature);
    )

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class TraderComplaint
{
public:
    uint160 message_hash;
    uint8_t side;
    Signature signature;

    TraderComplaint() { }

    static string_t Type() { return string_t("trader_complaint"); }

    TraderComplaint(uint160 message_hash, 
                    uint8_t side):
        message_hash(message_hash),
        side(side)
    { }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== TraderComplaint =========\n"
           << "== Message Hash: " 
           << message_hash.ToString() << "\n"
           << "== Side: " << (side == BID ? "bid" : "ask") << "\n"
           << "== Position: " << Position() << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End TraderComplaint =======\n";
        return ss.str();
    }

    uint64_t Position() const
    {
        TradeSecretMessage msg = msgdata[message_hash]["secret"];
        return msg.position;
    }

    DEPENDENCIES(message_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(message_hash);
        READWRITE(side);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        TradeSecretMessage msg = msgdata[message_hash]["secret"];
        AcceptCommit accept_commit;
        accept_commit = msgdata[msg.accept_commit_hash]["accept_commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];
        if (order.side == side)
            return order.VerificationKey();
        else
            return accept_commit.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class RefutationOfTraderComplaint
{
public:
    uint160 complaint_hash;
    CBigNum good_secret;
    Signature signature;

   RefutationOfTraderComplaint () { }

    static string_t Type() { return string_t("trader_complaint_refutation"); }

    RefutationOfTraderComplaint(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    {
        good_secret = keydata[GetPubKey()]["privkey"];
    }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== RefutationOfTraderComplaint =========\n"
           << "== Accept Commit Hash: " 
           << GetAcceptCommitHash().ToString() << "\n"
           << "== Secret Message Hash: " 
           << GetSecretMessage().GetHash160().ToString() << "\n"
           << "== Complaint Hash: "
           << GetComplaint().GetHash160().ToString() << "\n"
           << "== Good Secret: " << good_secret.ToString() << "\n"
           << "== Point: " << GetPubKey().ToString() << "\n"
           << "== Secret * Group Generator: "
           << Point(SECP256K1, good_secret).ToString() << "\n"
           << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End RefutationOfTraderComplaint =======\n";
        return ss.str();
    }

    TraderComplaint GetComplaint() const
    {
        return msgdata[complaint_hash]["trader_complaint"];
    }

    TradeSecretMessage GetSecretMessage() const
    {
        return msgdata[GetComplaint().message_hash]["secret"];
    }

    uint160 GetAcceptCommitHash() const
    {
        return GetSecretMessage().accept_commit_hash;
    }

    Point GetPubKey() const
    {
        uint160 hash = GetAcceptCommitHash();
        AcceptCommit accept_commit = msgdata[hash]["accept_commit"];

        OrderCommit commit;
        commit = msgdata[accept_commit.order_commit_hash]["commit"];
        Order order = msgdata[commit.order_hash]["order"];

        TradeSecretMessage secret_msg = GetSecretMessage();

        DistributedTradeSecret secret;

        uint8_t complaint_side = GetComplaint().side;

        if (secret_msg.direction == FORWARD)
        {
            secret = (order.side == complaint_side)
                     ? commit.distributed_trade_secret
                     : accept_commit.distributed_trade_secret;
        }
        else if (secret_msg.direction == BACKWARD)
        {
            secret = (order.side == complaint_side)
                     ? accept_commit.distributed_trade_secret
                     : commit.distributed_trade_secret;
        }
        PieceOfSecret piece = secret.pieces[secret_msg.position];

        return complaint_side == ASK ? piece.credit_pubkey 
                                     : piece.currency_pubkey;
    }

    bool Validate()
    {
        Point pubkey = GetPubKey();
        return Point(pubkey.curve, good_secret) == pubkey;
    }

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(good_secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        TraderComplaint complaint
            = msgdata[complaint_hash]["trader_complaint"];
        TradeSecretMessage msg = msgdata[complaint.message_hash]["secret"];
        return msg.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


class NewRelayChoiceMessage
{
public:
    uint160 accept_commit_hash;
    uint160 credit_hash;
    uint8_t chooser_side;
    std::vector<uint32_t> positions;
    std::vector<RevelationOfPieceOfSecret> revelations;
    Signature signature;

    NewRelayChoiceMessage() { }

    static string_t Type() { return string_t("new_relays"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n=========== NewRelayChoiceMessage =========\n"
           << "== Accept Commit Hash: " 
           << accept_commit_hash.ToString() << "\n"
           << "== Credit Hash: " << credit_hash.ToString() << "\n"
           << "== Chooser side: " 
           << (chooser_side == BID ? "bid" : "ask") << "\n"
           << "==\n"
           << "== Positions:\n";
        foreach_(uint32_t position, positions)
            ss << "== " << position << "\n";
        ss << "==\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "========= End NewRelayChoiceMessage =======\n";
        return ss.str();
    }

    NewRelayChoiceMessage(uint160 accept_commit_hash, 
                          uint160 credit_hash,
                          uint8_t side):
        accept_commit_hash(accept_commit_hash),
        credit_hash(credit_hash),
        chooser_side(side)
    {
        AcceptCommit accept_commit
            = msgdata[accept_commit_hash]["accept_commit"];
        OrderCommit commit
            = msgdata[accept_commit.order_commit_hash]["commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];
        SetPositionsAndSecrets(order, commit, accept_commit);
    }

    void SetPositionsAndSecrets(Order order, 
                                OrderCommit commit,
                                AcceptCommit accept_commit)
    {
        DistributedTradeSecret my_secret, their_secret;

        my_secret = (order.side == chooser_side)
                    ? commit.distributed_trade_secret
                    : accept_commit.distributed_trade_secret;

        their_secret = (order.side == chooser_side)
                        ? accept_commit.distributed_trade_secret
                        : commit.distributed_trade_secret;
   
        SetPositionsAndSecretsFromDistSecrets(my_secret, their_secret);
    }

    std::vector<Point> Relays()
    {
        return ChooseRelays(credit_hash, accept_commit_hash, 
                            credit_hash, positions.size());
    }

    void SetPositionsAndSecretsFromDistSecrets(
        DistributedTradeSecret my_secret,
        DistributedTradeSecret their_secret)
    {
        for (uint32_t i = 0; i < their_secret.relays.size(); i++)
        {
            Point pubkey = chooser_side == BID
                           ? their_secret.pieces[i].currency_pubkey
                           : their_secret.pieces[i].credit_pubkey;
            
            if (!keydata[pubkey].HasProperty("privkey"))
                positions.push_back(i);
        }
        
        std::vector<Point> relays = Relays();

        for (uint32_t i = 0; i < positions.size(); i++)
        {
            Point pubkey = my_secret.pieces[i].credit_pubkey;
            CBigNum secret = keydata[pubkey]["privkey"];
            RevelationOfPieceOfSecret revelation(relays[i], secret);
            revelations.push_back(revelation);
        }
    }

    DEPENDENCIES(accept_commit_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(accept_commit_hash);
        READWRITE(credit_hash);
        READWRITE(chooser_side);
        READWRITE(positions);
        READWRITE(revelations);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        AcceptCommit accept_commit
            = msgdata[accept_commit_hash]["accept_commit"];
        Order order = msgdata[accept_commit.order_hash]["order"];
        if (chooser_side == order.side)
            return order.VerificationKey();
        else
            return accept_commit.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

#endif