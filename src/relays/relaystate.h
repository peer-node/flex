// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef TELEPORT_SHAREDCHAIN
#define TELEPORT_SHAREDCHAIN

#include "crypto/uint256.h"
#include "crypto/point.h"
#include "database/data.h"
#include "crypto/hash.h"
#include "credits/credits.h"
#include <sstream>

#include <map>
#include <set>
#include <vector>
#include <deque>
#include <algorithm>
#include <boost/foreach.hpp>

#define NUM_EXECUTORS 6

#ifndef _PRODUCTION_BUILD

#define EXECUTOR_OFFSETS {2, 3, 4, 5, 6, 7}

#define MAX_RELAYS_REMEMBERED 40
#define MAX_POOL_SIZE 20
#define FUTURE_SUCCESSOR_FREQUENCY 10

#else

#define EXECUTOR_OFFSETS {9, 17, 19, 24, 31, 36}
#define MAX_RELAYS_REMEMBERED 1200
#define MAX_POOL_SIZE 600
#define FUTURE_SUCCESSOR_FREQUENCY 30

#endif

#include "log.h"
#define LOG_CATEGORY "relays/relaystate.h"

typedef std::map<Point, uint32_t> Numbering;
typedef std::map<Point, Point> PointMap;
typedef std::map<Point, std::set<uint160> > PointToMsgMap;
typedef std::map<uint160, Point> HashToPointMap;

extern uint32_t executor_offsets[6];


class RelayStateError : public std::runtime_error
{
public:
    explicit RelayStateError(const std::string& msg)
      : std::runtime_error(msg)
    { }
};

class RecoverHashesFailedException : public std::runtime_error
{
public:
    uint160 credit_hash;
    explicit RecoverHashesFailedException(const uint160& credit_hash)
      : std::runtime_error(credit_hash.ToString()),
        credit_hash(credit_hash)
    { }
};

class NoKnownRelayStateException : public std::runtime_error
{
public:
    uint160 earliest_credit_hash;
    explicit NoKnownRelayStateException(const uint160& earliest_credit_hash)
      : std::runtime_error(earliest_credit_hash.ToString()),
        earliest_credit_hash(earliest_credit_hash)
    { }
};

inline uint160 PointListHash(std::vector<Point> points)
{
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << points;
    return Hash160(ss.begin(), ss.end());
}

class RelayState
{
public:
    Numbering relays;
    PointMap line_of_succession;
    PointMap actual_successions;
    std::map<Point, std::set<uint160> > subthreshold_succession_msgs;
    std::map<Point, std::set<uint160> > disqualifications;
    std::map<uint160, Point> join_hash_to_relay;
    uint160 latest_credit_hash;

    uint32_t latest_relay_number;

    RelayState():
        latest_relay_number(0)
    { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relays);
        READWRITE(join_hash_to_relay);
        READWRITE(line_of_succession);
        READWRITE(actual_successions);
        READWRITE(subthreshold_succession_msgs);
        READWRITE(disqualifications);
        READWRITE(latest_relay_number);
        READWRITE(latest_credit_hash);
    )

    string_t ToString() const;

    uint160 GetHash160() const;

    uint32_t AddRelay(Point relay, uint160 join_hash);

    void AddDisqualification(Point relay, uint160 message_hash);

    void RemoveRelay(Point relay, bool decrement=true);

    void RemoveRelayFromSuccessors(Point relay);

    void TrimRelays();

    Point Relay(uint32_t relay_number);

    uint160 GetJoinHash(Point relay);

    Point OldestRelay();

    std::map<uint32_t, Point> RelaysByNumber();

    std::vector<Point> Executors(uint32_t relay_number);
    
    std::vector<Point> Executors(Point relay);

    Point Successor(uint32_t relay_number);

    Point Successor(Point relay);

    Point DeadPredecessor(Point relay);

    Point SurvivingSuccessor(Point relay);

    std::vector<Point> GetEligibleRelays();

    Point SelectRelay(uint64_t chooser_number);

    uint32_t NumberOfValidRelays();

    void SetLatestCreditHash(uint160 credit_hash);

    std::vector<Point> ChooseRelays(uint160 chooser, uint32_t num_relays);
};

template <typename S>
class SharedChain
{
public:
    uint160 previous_state_hash;
    std::vector<uint160> credit_hashes;
    uint160 state_hash;
    S state;

    std::vector<string_t> message_types;

    SharedChain():
        previous_state_hash(0),
        state_hash(0)
    { }

    SharedChain(S state):
        state(state)
    {
        previous_state_hash = GetHash160();
    }
    
    SharedChain(uint160 previous_state_hash):
        previous_state_hash(previous_state_hash)
    {
        state = relaydata[previous_state_hash]["state"]; 
    }

    ~SharedChain()
    { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(previous_state_hash);
        READWRITE(credit_hashes);
        READWRITE(state_hash);
    )

    virtual void HandleMessage(uint160 message_hash) { }

    uint160 GetHash160()
    {
        return 0;
    }

    void HandleBatch(uint160 credit_hash);
};

class RelayStateChain : public SharedChain<RelayState>
{
public:
    std::set<uint160> handled_messages;

    RelayStateChain()
    {
        SetMessages();
    }

    RelayStateChain(RelayState state): SharedChain<RelayState>(state)
    {
        SetMessages();
    }

    RelayStateChain(uint160 previous_state_hash):
        SharedChain<RelayState>(previous_state_hash)
    {
        SetMessages();
    }

    ~RelayStateChain() { }

    void SetMessages()
    {
        message_types.push_back("join");
        message_types.push_back("leave");
        message_types.push_back("successor");
        message_types.push_back("succession");
        message_types.push_back("trader_complaint");
        message_types.push_back("complaint_refutation");
    }

    uint160 GetHash160()
    {
        return state.GetHash160();
    }
    
    bool ValidateBatch(uint160 credit_hash);

    void HandleMessage(uint160 message_hash)
    {
        
        string_t type = msgdata[message_hash]["type"];
        
        if (type == "join")
        {
            log_ << "handling join\n";
            HandleJoin(message_hash);
        }
        if (type == "leave")
            HandleLeave(message_hash);
        if (type == "successor")
            HandleSuccessor(message_hash);
        if (type == "succession")
            HandleSuccession(message_hash);
        if (type == "trader_complaint")
            HandleTraderComplaint(message_hash);
        if (type == "trader_complaint_refutation")
            HandleRefutationOfTraderComplaint(message_hash);
        if (type == "relay_complaint_refutation")
            HandleRefutationOfRelayComplaint(message_hash);
        handled_messages.insert(message_hash);
        
        log_ << "RelayStateChain finished handling message "
             << message_hash << "\n";
    }

    bool HandleMessages(std::vector<uint160> messages);

    bool HandleMessages(std::vector<uint160> messages,
                        std::vector<uint160>& rejected);

    void UnHandleMessage(uint160 message_hash)
    {
        string_t type = msgdata[message_hash]["type"];
        if (type == "join")
            UnHandleJoin(message_hash);
        if (type == "leave")
            UnHandleLeave(message_hash);
        if (type == "successor")
            UnHandleSuccessor(message_hash);
        if (type == "succession")
            UnHandleSuccession(message_hash);
        if (type == "trader_complaint")
            UnHandleTraderComplaint(message_hash);
        if (type == "trader_complaint_refutation")
            UnHandleRefutationOfTraderComplaint(message_hash);
        if (type == "relay_complaint_refutation")
            UnHandleRefutationOfRelayComplaint(message_hash);
        
        handled_messages.erase(message_hash);
    }

    bool ValidateMessage(uint160 message_hash)
    {
        string_t type = msgdata[message_hash]["type"];
        if (type == "join")
            return ValidateJoin(message_hash);
        if (type == "leave")
            return ValidateLeave(message_hash);
        if (type == "successor")
            return ValidateSuccessor(message_hash);
        if (type == "succession")
            return ValidateSuccession(message_hash);
        if (type == "trader_complaint")
            return ValidateTraderComplaint(message_hash);
        if (type == "trader_complaint_refutation")
            return ValidateRefutationOfTraderComplaint(message_hash);
        if (type == "relay_complaint_refutation")
            return ValidateRefutationOfRelayComplaint(message_hash);
        return true;
    }

    bool ValidateJoin(uint160 message_hash);
        
    bool ValidateLeave(uint160 message_hash);

    bool ValidateSuccessor(uint160 message_hash);

    bool ValidateSuccession(uint160 message_hash);

    void HandleComplaintFromFutureSuccessor(uint160 message_hash);

    void UnHandleComplaintFromFutureSuccessor(uint160 message_hash);

    void HandleRefutationOfComplaintFromFutureSuccessor(uint160 message_hash);

    void UnHandleRefutationOfComplaintFromFutureSuccessor(
        uint160 message_hash);

    bool ValidateTraderComplaint(uint160 message_hash);

    bool ValidateRefutationOfTraderComplaint(uint160 message_hash);

    bool ValidateRefutationOfRelayComplaint(uint160 message_hash);

    void HandleJoin(uint160 message_hash);

    void HandleLeave(uint160 message_hash);

    void HandleSuccessor(uint160 message_hash);

    void HandleSuccession(uint160 message_hash);

    void HandleTraderComplaint(uint160 message_hash);
    
    void HandleRefutationOfTraderComplaint(uint160 message_hash);

    void HandleRefutationOfRelayComplaint(uint160 message_hash);

    void UnHandleJoin(uint160 message_hash);

    void UnHandleLeave(uint160 message_hash);

    void UnHandleSuccessor(uint160 message_hash);

    void UnHandleSuccession(uint160 message_hash);

    void UnHandleTraderComplaint(uint160 message_hash);

    void UnHandleRefutationOfTraderComplaint(uint160 message_hash);

    void UnHandleRefutationOfRelayComplaint(uint160 message_hash);

    void AddDisqualification(Point relay, uint160 message_hash);

    void RemoveDisqualification(Point relay, uint160 message_hash);
};


RelayState GetRelayState(uint160 credit_hash);

void RecordRelayState(RelayState state, uint160 credit_hash);


#endif
