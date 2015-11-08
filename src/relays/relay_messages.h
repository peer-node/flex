#ifndef FLEX_RELAY_MESSAGES
#define FLEX_RELAY_MESSAGES

#include "database/data.h"
#include "crypto/flexcrypto.h"
#include "crypto/shorthash.h"
#include "credits/credits.h"
#include "trade/trade.h"
#include "flexnode/schedule.h"
#include "relays/relaystate.h"
#include "relays/successionsecret.h"
#include "flexnode/message_handler.h"

#include "log.h"
#define LOG_CATEGORY "relay_messages.h"

DistributedSuccessionSecret GetDistributedSuccessionSecret(Point relay);



class RelayJoinMessage
{
public:
    uint32_t relay_number;
    uint160 credit_hash;
    uint160 preceding_relay_join_hash;
    Point relay_pubkey;
    CBigNum successor_secret_xor_shared_secret;
    Point successor_secret_point;
    DistributedSuccessionSecret distributed_succession_secret;
    Signature signature;

    RelayJoinMessage():
        relay_number(0)
    { }

    static string_t Type() { return string_t("join"); }

    RelayJoinMessage(uint160 credit_hash);

    string_t ToString() const
    {
        stringstream ss;
        ss << "\n============== RelayJoinMessage =============" << "\n"
           << "== Credit Hash: " << credit_hash.ToString() << "\n"
           << "==" << "\n"
           << "== Relay Number:" << relay_number << "\n"
           << "== Preceding Join Hash: "
           << preceding_relay_join_hash.ToString() << "\n"
           << "== Relay Pubkey:" << relay_pubkey.ToString() << "\n"
           << "== Distributed Succession Secret: " 
           << distributed_succession_secret.ToString()
           << "==" << "\n"
           << "== Successor Secret xor Shared Secret: " 
           << successor_secret_xor_shared_secret.ToString() << "\n"
           << "== Successor Secret Point: " 
           << successor_secret_point.ToString() << "\n"
           << "==" << "\n"
           << "== Hash: " << GetHash160().ToString() << "\n"
           << "============ End RelayJoinMessage ===========" << "\n";
        return ss.str();
    }

    bool operator<(const RelayJoinMessage& other) const
    {
        return relay_number < other.relay_number;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relay_number);
        READWRITE(credit_hash);
        READWRITE(preceding_relay_join_hash);
        READWRITE(relay_pubkey);
        READWRITE(successor_secret_xor_shared_secret);
        READWRITE(successor_secret_point);
        READWRITE(distributed_succession_secret);
        READWRITE(signature);
    )

    DEPENDENCIES(credit_hash, preceding_relay_join_hash);

    Point VerificationKey() const
    {
        MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
        Point pubkey;
        pubkey.setvch(mined_credit.keydata);
        return pubkey;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

RelayJoinMessage GetJoinMessage(Point relay);

class JoinComplaintFromSuccessor
{
public:
    uint160 join_hash;
    Signature signature;

    JoinComplaintFromSuccessor() { }

    JoinComplaintFromSuccessor(uint160 join_hash):
        join_hash(join_hash)
    { }

    static string_t Type() { return string_t("join_complaint_successor"); }

    string_t ToString() const
    {
        std::stringstream ss;
        ss << "=============== JoinComplaintFromSuccessor ===========\n"
           << "== Join hash: " << join_hash.ToString() << "\n"
           << "== Successor: " << VerificationKey().ToString() << "\n"
           << "============= End JoinComplaintFromSuccessor =========\n";
        return ss.str();
    }

    DEPENDENCIES(join_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(join_hash);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        RelayJoinMessage join = msgdata[join_hash]["join"];
        Point successor;
        if (join.relay_number % 60)
        {
            RelayJoinMessage successor_join
                = msgdata[join.preceding_relay_join_hash]["join"];
            successor = join.relay_pubkey;
        }
        return successor;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class RefutationOfJoinComplaintFromSuccessor
{
public:
    uint160 complaint_hash;
    CBigNum secret;
    Signature signature;

    RefutationOfJoinComplaintFromSuccessor() { }

    static string_t Type() { return string_t("successor_complaint_refutation"); }

    RefutationOfJoinComplaintFromSuccessor(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    {
        secret = keydata[GetPoint()]["privkey"];
    }

    string_t ToString() const
    {
        std::stringstream ss;
        RelayJoinMessage join = GetJoin();

        ss << "========== RefutationOfJoinComplaintFromSuccessor ========\n"
           << "== Complaint hash: " << complaint_hash.ToString() << "\n"
           << "== Join hash: " << join.GetHash160().ToString() << "\n"
           << "== Successor Secret: " << secret.ToString() << "\n"
           << "== Successor Secret Point from Join: " 
           << join.successor_secret_point.ToString() << "\n"
           << "== Secret * Group Generator: " 
           << Point(SECP256K1, secret).ToString() << "\n"
           << "======== End RefutationOfJoinComplaintFromSuccessor ======\n";
        return ss.str();
    }

    RelayJoinMessage GetJoin() const
    {
        return msgdata[GetComplaint().join_hash]["join"];
    }

    JoinComplaintFromSuccessor GetComplaint() const
    {
        return msgdata[complaint_hash]["join_complaint_successor"];
    }

    Point GetPoint() const
    {
        return GetJoin().successor_secret_point;
    }

    Point GetRecipient() const
    {
        return GetComplaint().VerificationKey();
    }

    bool Validate()
    {
        Point recipient = GetRecipient();
        CBigNum shared_secret = Hash(secret * recipient);

        RelayJoinMessage join = GetJoin();

        return (Point(SECP256K1, secret) == join.successor_secret_point) &&
                (join.successor_secret_xor_shared_secret ^ shared_secret)
                == secret;
    }

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        return GetJoin().VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class JoinComplaintFromExecutor
{
public:
    uint160 join_hash;
    uint32_t position;
    Signature signature;

    JoinComplaintFromExecutor() { }

    JoinComplaintFromExecutor(uint160 join_hash, uint32_t position):
        join_hash(join_hash),
        position(position)
    { }

    static string_t Type() { return string_t("join_complaint_executor"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "========== JoinComplaintFromExecutor ========\n"
           << "== Join Hash: " << join_hash.ToString() << "\n"
           << "== Position: " << position << "\n"
           << "== Executor: " << VerificationKey().ToString() << "\n"
           << "======== End JoinComplaintFromExecutor ======\n";
        return ss.str();
    }

    DEPENDENCIES(join_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(join_hash);
        READWRITE(position);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        std::vector<Point> relays;
        
        RelayJoinMessage join = msgdata[join_hash]["join"];
        relays = join.distributed_succession_secret.Relays();
        
        if (relays.size() <= position)
            return Point(SECP256K1);
        
        return relays[position];
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class RefutationOfJoinComplaintFromExecutor
{
public:
    uint160 complaint_hash;
    CBigNum secret;
    Signature signature;

    RefutationOfJoinComplaintFromExecutor() { }

    static string_t Type() 
    {
        return string_t("executor_complaint_refutation");
    }

    RefutationOfJoinComplaintFromExecutor(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    {
        secret = keydata[GetPoint()]["privkey"];
    }

    string_t ToString() const
    {
        std::stringstream ss;
        RelayJoinMessage join = GetJoin();

        ss << "========== RefutationOfJoinComplaintFromExecutor ========\n"
           << "== Join hash: " << join.GetHash160().ToString() << "\n"
           << "== Complaint hash: " << complaint_hash.ToString() << "\n"
           << "== Executor Secret: " << secret.ToString() << "\n"
           << "== Executor Position: " << GetPosition() << "\n"
           << "== Executor Secret Point from Join: " 
           << GetPoint().ToString() << "\n"
           << "== Secret * Group Generator: " 
           << Point(SECP256K1, secret).ToString() << "\n"
           << "======== End RefutationOfJoinComplaintFromExecutor ======\n";
        return ss.str();
    }

    RelayJoinMessage GetJoin() const
    {
        return msgdata[GetComplaint().join_hash]["join"];
    }

    JoinComplaintFromExecutor GetComplaint() const
    {
        return msgdata[complaint_hash]["join_complaint_executor"];
    }

    int32_t GetPosition() const
    {
        return GetComplaint().position;
    }

    Point GetPoint() const
    {
        int32_t position = GetPosition();
        return GetJoin().distributed_succession_secret.point_values[position];
    }

    Point GetRecipient() const
    {
        int32_t position = GetPosition();
        return GetJoin().distributed_succession_secret.Relays()[position];
    }

    bool Validate()
    {
        Point recipient = GetRecipient();
        CBigNum shared_secret = Hash(secret * recipient);

        RelayJoinMessage join = GetJoin();

        return (Point(SECP256K1, secret) == GetPoint()) &&
                (join.successor_secret_xor_shared_secret ^ shared_secret)
                == secret;
    }

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        return GetJoin().VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class SuccessionMessage
{
public:
    uint160 dead_relay_join_hash;
    uint160 successor_join_hash;
    Point executor;
    CBigNum succession_secret_xor_shared_secret;
    Signature signature;

    SuccessionMessage() { }

    static string_t Type() { return string_t("succession"); }

    SuccessionMessage(Point dead_relay, Point executor);

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n========== SuccessionMessage ========\n"
           << "== Dead Relay Join Hash: " 
           << dead_relay_join_hash.ToString() << "\n"
           << "== Successor Join Hash: " 
           << successor_join_hash.ToString() << "\n"
           << "== Executor: " << executor.ToString() << "\n"
           << "== Executor Position: " << Position() << "\n"
           << "== Succession Secret ^ Shared Secret: " 
           << succession_secret_xor_shared_secret.ToString() << "\n"
           << "======== End SuccessionMessage ======\n";
        return ss.str();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(dead_relay_join_hash);
        READWRITE(successor_join_hash);
        READWRITE(executor);
        READWRITE(succession_secret_xor_shared_secret);
        READWRITE(signature);
    )

    DEPENDENCIES();

    Point Successor() const
    {
        RelayJoinMessage join = msgdata[successor_join_hash]["join"];
        return join.relay_pubkey;
    }

    Point Deceased() const
    {
        RelayJoinMessage join = msgdata[dead_relay_join_hash]["join"];
        return join.relay_pubkey;
    }

    Point VerificationKey() const
    {
        if (Position() == -1)
            return Point(SECP256K1, 0);

        return executor;
    }

    int64_t Position() const
    {
        RelayJoinMessage join = msgdata[dead_relay_join_hash]["join"];
        return join.distributed_succession_secret.Position(executor);
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class SuccessionComplaint
{
public:
    uint160 succession_msg_hash;
    Signature signature;

    SuccessionComplaint() { }

    SuccessionComplaint(uint160 succession_msg_hash):
        succession_msg_hash(succession_msg_hash)
    { }

    static string_t Type() { return string_t("succession_complaint"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n========== SuccessionComplaint ========\n"
           << "== Dead Relay Join Hash: " << JoinHash().ToString() << "\n"
           << "== Succession Msg Hash: " 
           << succession_msg_hash.ToString() << "\n"
           << "== Executor: " 
           << GetSuccessionMessage().VerificationKey().ToString() << "\n"
           << "== Executor Position: " << Position() << "\n"
           << "======== End SuccessionComplaint ======\n";
        return ss.str();
    }

    DEPENDENCIES(succession_msg_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(succession_msg_hash);
        READWRITE(signature);
    )

    SuccessionMessage GetSuccessionMessage() const
    {
        return msgdata[succession_msg_hash]["succession"];
    }

    uint160 JoinHash() const
    {
        return GetSuccessionMessage().successor_join_hash;
    }

    uint64_t Position() const
    {
        return GetSuccessionMessage().Position();
    }

    Point VerificationKey() const
    {
        RelayJoinMessage successor_join = msgdata[JoinHash()]["join"];
        return successor_join.VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class RefutationOfSuccessionComplaint
{
public:
    uint160 complaint_hash;
    CBigNum secret;
    Signature signature;

    RefutationOfSuccessionComplaint() { }

    static string_t Type() 
    { 
        return string_t("succession_complaint_refutation");
    }

    RefutationOfSuccessionComplaint(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    {
        secret = keydata[GetPoint()]["privkey"];
    }
    
    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n========== RefutationOfSuccessionComplaint ========\n"
           << "== Complaint Hash: " << complaint_hash.ToString() << "\n"
           << "== Dead Relay Join Hash: " 
           << GetJoin().GetHash160().ToString() << "\n"
           << "== Succession Msg Hash: " 
           << GetSuccessionMessage().GetHash160().ToString() << "\n"
           << "== Executor: " 
           << GetSuccessionMessage().VerificationKey().ToString() << "\n"
           << "== Executor Position: " << Position() << "\n"
           << "== Executor Secret: " << secret.ToString() << "\n"
           << "== Executor Point: " << GetPoint().ToString() << "\n"
           << "== Executor Secret * Group Generator: "
           << Point(SECP256K1, secret).ToString() << "\n"
           << "======== End RefutationOfSuccessionComplaint ======\n";
        return ss.str();
    }

    RelayJoinMessage GetJoin() const
    {
        RelayJoinMessage join = msgdata[GetComplaint().JoinHash()]["join"];
        return join;
    }

    SuccessionComplaint GetComplaint() const
    {
        SuccessionComplaint complaint
            = msgdata[complaint_hash]["succession_complaint"];
        return complaint;
    }

    SuccessionMessage GetSuccessionMessage() const
    {
        SuccessionMessage msg
            = msgdata[GetComplaint().succession_msg_hash]["succession"];
        return msg;
    }

    uint32_t Position() const
    {
        Point relay = GetSuccessionMessage().VerificationKey();
        return GetJoin().distributed_succession_secret.RelayPosition(relay);
    }

    Point GetPoint() const
    {
        int32_t position = Position();
        return GetJoin().distributed_succession_secret.point_values[position];
    }

    Point GetRecipient() const
    {   
        return GetComplaint().VerificationKey();
    }

    bool Validate()
    {
        Point recipient = GetRecipient();
        CBigNum shared_secret = Hash(secret * recipient);

        RelayJoinMessage join = GetJoin();

        return (Point(SECP256K1, secret) == join.successor_secret_point) &&
               (join.successor_secret_xor_shared_secret ^ shared_secret)
                == secret;
    }

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        return GetJoin().VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class FutureSuccessorSecretMessage
{
public:
    uint160 credit_hash;
    uint160 successor_join_hash;
    CBigNum successor_secret_xor_shared_secret;
    Signature signature;

    FutureSuccessorSecretMessage() { }

    static string_t Type() { return string_t("successor"); }

    FutureSuccessorSecretMessage(uint160 credit_hash, Point successor);

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n========== FutureSuccessorSecretMessage ========\n"
           << "== Credit Hash: " << credit_hash.ToString() << "\n"
           << "== Join Hash: " << JoinHash().ToString() << "\n"
           << "== Successor Join Hash: " 
           << successor_join_hash.ToString() << "\n"
           << "== Successor Secret ^ Shared Secret: " 
           << successor_secret_xor_shared_secret.ToString() << "\n"
           << "======== End FutureSuccessorSecretMessage ======\n";
        return ss.str();
    }

    DEPENDENCIES(successor_join_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_hash);
        READWRITE(successor_join_hash);
        READWRITE(successor_secret_xor_shared_secret);
        READWRITE(signature);
    )

    uint160 JoinHash() const
    {
        return relaydata[credit_hash]["accepted_join"]; 
    }

    Point VerificationKey() const
    {
        MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
        Point pubkey;
        pubkey.setvch(mined_credit.keydata);
        return pubkey;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class ComplaintFromFutureSuccessor
{
public:
    uint160 future_successor_msg_hash;
    Signature signature;

    ComplaintFromFutureSuccessor() { }

    ComplaintFromFutureSuccessor(uint160 future_successor_msg_hash):
        future_successor_msg_hash(future_successor_msg_hash)
    { }

    static string_t Type() { return string_t("complaint_future_successor"); }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n========== ComplaintFromFutureSuccessor ========\n"
           << "== Successor Msg Hash: " 
           << future_successor_msg_hash.ToString() << "\n"
           << "== Join Hash: " << JoinHash().ToString() << "\n"
           << "== Successor: " << VerificationKey().ToString() << "\n"
           << "======== End ComplaintFromFutureSuccessor ======\n";
        return ss.str();
    }

    DEPENDENCIES(future_successor_msg_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(future_successor_msg_hash);
        READWRITE(signature);
    )

    FutureSuccessorSecretMessage GetFutureSuccessorSecretMessage() const
    {
        return msgdata[future_successor_msg_hash]["successor"];
    }

    uint160 JoinHash() const
    {
        return GetFutureSuccessorSecretMessage().JoinHash(); 
    }

    Point VerificationKey() const
    {
        RelayJoinMessage join = msgdata[JoinHash()]["join"];
        return join.relay_pubkey;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class RefutationOfComplaintFromFutureSuccessor
{
public:
    uint160 complaint_hash;
    CBigNum secret;
    Signature signature;

    RefutationOfComplaintFromFutureSuccessor() { }

    static string_t Type() { return string_t("future_successor_refutation"); }

    RefutationOfComplaintFromFutureSuccessor(uint160 complaint_hash):
        complaint_hash(complaint_hash)
    {
        secret = keydata[GetPoint()]["privkey"];
    }

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n========== RefutationOfComplaintFromFutureSuccessor ========\n"
           << "== Complaint Hash: " << complaint_hash.ToString() << "\n"
           << "== Join Hash: " << GetJoin().GetHash160().ToString() << "\n"
           << "== Successor: " << GetRecipient().ToString() << "\n"
           << "== Successor Secret: " << secret.ToString() << "\n"
           << "== Successor Point: " << GetPoint().ToString() << "\n"
           << "== Successor Secret * Group Generator: " 
           << Point(SECP256K1, secret).ToString() << "\n"
           << "======== End RefutationOfComplaintFromFutureSuccessor ======\n";
        return ss.str();
    }

    RelayJoinMessage GetJoin() const
    {
        return msgdata[GetComplaint().JoinHash()]["join"];
    }

    ComplaintFromFutureSuccessor GetComplaint() const
    {
        return msgdata[complaint_hash]["join_complaint_successor"];
    }

    Point GetPoint() const
    {
        return GetJoin().successor_secret_point;
    }

    Point GetRecipient() const
    {
        return GetComplaint().VerificationKey();
    }

    bool Validate()
    {
        Point recipient = GetRecipient();
        CBigNum shared_secret = Hash(secret * recipient);

        RelayJoinMessage join = GetJoin();

        return (Point(SECP256K1, secret) == join.successor_secret_point) &&
               (join.successor_secret_xor_shared_secret ^ shared_secret)
                == secret;
    }

    DEPENDENCIES(complaint_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(complaint_hash);
        READWRITE(secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        return GetJoin().VerificationKey();
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class RelayLeaveMessage
{
public:
    Point relay_leaving;
    Point successor;
    CBigNum succession_secret_xor_shared_secret;
    Signature signature;

    RelayLeaveMessage() { }

    static string_t Type() { return string_t("relay_leave"); }

    RelayLeaveMessage(Point relay_leaving);

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n========== RelayLeaveMessage ========\n"
           << "== Relay Leaving: " 
           << relay_leaving.ToString() << "\n"
           << "== Successor: " << successor.ToString() << "\n"
           << "== Succession Secret ^ Shared Secret: "
           << succession_secret_xor_shared_secret.ToString() << "\n"
           << "======== End RelayLeaveMessage ======\n";
        return ss.str();
    }

    DEPENDENCIES();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relay_leaving);
        READWRITE(successor);
        READWRITE(succession_secret_xor_shared_secret);
        READWRITE(signature);
    )

    Point VerificationKey() const
    {
        return relay_leaving;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};

class RelayLeaveComplaint
{
public:
    uint160 leave_msg_hash;
    Signature signature;

    RelayLeaveComplaint() { }

    RelayLeaveComplaint(uint160 leave_msg_hash):
        leave_msg_hash(leave_msg_hash)
    { }
    
    static string_t Type() { return string_t("leave_complaint"); }

    string_t ToString() const
    {
        std::stringstream ss;
        RelayLeaveMessage leave = msgdata[leave_msg_hash]["relay_leave"];
        Point relay_leaving = leave.VerificationKey();
        ss << "\n========== RelayLeaveComplaint ========\n"
           << "== Leave Hash: " << leave_msg_hash.ToString() << "\n"
           << "== Relay Leaving: " << relay_leaving.ToString() << "\n"
           << "== Successor: " << VerificationKey().ToString() << "\n"
           << "======== End RelayLeaveComplaint ======\n";
        return ss.str();
    }
    
    DEPENDENCIES(leave_msg_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(leave_msg_hash);
        READWRITE(signature);
    )

    Point VerificationKey() const;

    IMPLEMENT_HASH_SIGN_VERIFY();
};

#endif