#ifndef FLEX_RELAYS
#define FLEX_RELAYS

#include "relays/relay_inheritance.h"
#include "relays/relay_admission.h"
#include "net/resourcemonitor.h"

#include "log.h"
#define LOG_CATEGORY "relays.h"

extern uint32_t executor_offsets[6];

void InitializeRelayTasks();

extern CCriticalSection relay_handler_mutex;

class RelayHandler : public MessageHandlerWithOrphanage
{
public:
    const char *channel;
    RelayStateChain relay_chain;

    uint160 latest_valid_join_hash;
    uint32_t relay_number_of_my_future_successor;
    uint160 credit_hash_awaiting_successor;
    
    RelayHandler()
    {
        channel = "relay";
        latest_valid_join_hash = 0;
    }

    RelayHandler(RelayState state)
    {
        channel = "relay";
        relay_chain = RelayStateChain(state);
        Point latest_relay = state.Relay(state.latest_relay_number);
        latest_valid_join_hash = state.GetJoinHash(latest_relay);
    }

    template<typename T>
    void BroadcastMessage(T message)
    {
        CDataStream ss = GetRelayBroadcastStream(message);
        uint160 message_hash = message.GetHash160();
        log_ << "broadcasting msg with hash " << message_hash << "\n";
            
        RelayRelayMessage(ss);
        if (!relaydata[message_hash]["received"])
            HandleMessage(ss, NULL);
    }

    template<typename T>
    CDataStream GetRelayBroadcastStream(T message)
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

    HandleClass_(RelayJoinMessage);
    HandleClass_(RelayLeaveMessage);
    HandleClass_(JoinComplaintFromSuccessor);
    HandleClass_(ComplaintFromFutureSuccessor);
    HandleClass_(JoinComplaintFromExecutor);
    HandleClass_(SuccessionComplaint);
    HandleClass_(RefutationOfJoinComplaintFromSuccessor);
    HandleClass_(RefutationOfComplaintFromFutureSuccessor);
    HandleClass_(RefutationOfJoinComplaintFromExecutor);
    HandleClass_(RefutationOfSuccessionComplaint);
    HandleClass_(FutureSuccessorSecretMessage);
    HandleClass_(SuccessionMessage);


    void HandleMessage(CDataStream ss, CNode *peer)
    {
        string message_type;
        ss >> message_type;
        ss >> message_type;

        uint160 message_hash = Hash160(ss.begin(), ss.end());
        relaydata[message_hash]["received"] = true;
        msgdata[message_hash]["type"] = message_type;
        should_forward = true;

        handle_stream_(RelayJoinMessage, ss, peer);
        handle_stream_(RelayLeaveMessage, ss, peer);
        handle_stream_(JoinComplaintFromSuccessor, ss, peer);
        handle_stream_(ComplaintFromFutureSuccessor, ss, peer);
        handle_stream_(JoinComplaintFromExecutor, ss, peer);
        handle_stream_(SuccessionComplaint, ss, peer);
        handle_stream_(RefutationOfJoinComplaintFromSuccessor, ss, peer);
        handle_stream_(RefutationOfComplaintFromFutureSuccessor, ss, peer);
        handle_stream_(RefutationOfJoinComplaintFromExecutor, ss, peer);
        handle_stream_(RefutationOfSuccessionComplaint, ss, peer);
        handle_stream_(FutureSuccessorSecretMessage, ss, peer);
        handle_stream_(SuccessionMessage, ss, peer);
        
    }

    void HandleMessage(uint160 message_hash)
    {
        string_t message_type = msgdata[message_hash]["type"];
        
        handle_hash_(RelayJoinMessage, message_hash);
        handle_hash_(RelayLeaveMessage, message_hash);
        handle_hash_(JoinComplaintFromSuccessor, message_hash);
        handle_hash_(ComplaintFromFutureSuccessor, message_hash);
        handle_hash_(JoinComplaintFromExecutor, message_hash);
        handle_hash_(SuccessionComplaint, message_hash);
        handle_hash_(RefutationOfJoinComplaintFromSuccessor, message_hash);
        handle_hash_(RefutationOfComplaintFromFutureSuccessor, message_hash);
        handle_hash_(RefutationOfJoinComplaintFromExecutor, message_hash);
        handle_hash_(RefutationOfSuccessionComplaint, message_hash);
        handle_hash_(FutureSuccessorSecretMessage, message_hash);
        handle_hash_(SuccessionMessage, message_hash);
    }

    void AddBatchToTip(MinedCreditMessage& msg);

    void UnHandleBatch();

    void UnHandleBatch(MinedCreditMessage& msg);

    bool JoinMessageIsCompatibleWithState(RelayJoinMessage& msg);

    void QueueJoin(RelayJoinMessage msg);

    void HandleQueuedJoins(uint160 credit_hash);

    bool ValidateRelayJoinMessage(RelayJoinMessage& msg);

    bool ValidateJoinSuccessionSecrets(RelayJoinMessage& msg);

    void HandleRelayJoinMessage(RelayJoinMessage msg);

    void HandleRelayLeaveMessage(RelayLeaveMessage msg);

    void HandleExecutorSecrets(RelayJoinMessage msg);

    void HandleExecutorSecret(RelayJoinMessage msg, Point executor);

    bool IsTooLateToComplain(uint160 join_hash);

    void HandleJoinComplaintFromSuccessor(JoinComplaintFromSuccessor complaint);
    
    void HandleComplaintFromFutureSuccessor(
        ComplaintFromFutureSuccessor complaint);

    void HandleJoinComplaintFromExecutor(JoinComplaintFromExecutor complaint);

    void HandleSuccessionComplaint(SuccessionComplaint complaint);

    void HandleRefutationOfJoinComplaintFromSuccessor(
        RefutationOfJoinComplaintFromSuccessor refutation);

    void HandleRefutationOfComplaintFromFutureSuccessor(
        RefutationOfComplaintFromFutureSuccessor refutation);

    void HandleRefutationOfJoinComplaintFromExecutor(
        RefutationOfJoinComplaintFromExecutor refutation);

    void HandleRefutationOfSuccessionComplaint(
        RefutationOfSuccessionComplaint refutation);
    
    void EjectApplicant(Point applicant);
    
    void HandleDownStreamApplicants(Point applicant);
    
    void ReJoin(Point applicant);
    
    void RecordDownStreamApplicant(Point applicant, Point downstream_applicant);

    vector<Point> GetDownstream(Point applicant);

    void GetDownstream(Point applicant, std::vector<Point>& downstream);
    
    void AcceptJoin(RelayJoinMessage msg);

    void StoreJoinMessage(RelayJoinMessage msg);

    bool HandleSuccessorSecrets(RelayJoinMessage msg);

    void HandleMyFutureSuccessor(RelayJoinMessage successor_join_msg);

    bool ValidateFutureSuccessorSecretMessage(FutureSuccessorSecretMessage msg);

    void HandleFutureSuccessorSecretMessage(FutureSuccessorSecretMessage msg);

    void HandleSuccessionMessage(SuccessionMessage msg);

    std::vector<Point> Executors(uint32_t relay_number);

    std::vector<Point> Executors(Point relay);

    Point Successor(uint32_t relay_number);

    Point Successor(Point relay);

    void ScheduleFutureSuccessor(uint160 credit_hash, uint32_t relay_number);

    RelayJoinMessage GetLatestValidJoin();
};

#endif
