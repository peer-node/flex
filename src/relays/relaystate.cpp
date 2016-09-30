#include "relays/relaystate.h"
#include "relays/relays.h"
#include "credits/creditsign.h"
#include "teleportnode/teleportnode.h"

#include "log.h"
#define LOG_CATEGORY "relaystate.cpp"

using namespace std;

bool BatchIsInDiurn(uint160 credit_hash, uint160 latest_credit_hash)
{
    if (credit_hash == latest_credit_hash)
        return true;
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
    if (mined_credit.previous_mined_credit_hash == latest_credit_hash)
        return true;
    while (!calendardata[latest_credit_hash]["is_calend"] &&
           latest_credit_hash != 0)
    {
        MinedCredit credit = creditdata[latest_credit_hash]["mined_credit"];
        latest_credit_hash = credit.previous_mined_credit_hash;
        if (credit_hash == latest_credit_hash)
            return true;
    }
    return false;
}

RelayState GetRelayState(uint160 credit_hash)
{
    log_ << "GetRelayState: looking for state for " << credit_hash << "\n";
    deque<uint160> credit_hashes;
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
    uint160 original_credit_hash = credit_hash;

    uint160 final_state_hash = mined_credit.relay_state_hash;
 
    while (!relaydata[credit_hash].HasProperty("state") 
           && mined_credit.relay_state_hash != 0)
    {
        credit_hashes.push_front(credit_hash);
        uint160 prev_hash = mined_credit.previous_mined_credit_hash;
        mined_credit = creditdata[prev_hash]["mined_credit"];
        
        credit_hash = prev_hash;
    }

    RelayState state;

    if (relaydata[credit_hash].HasProperty("state"))
    {
        state = relaydata[credit_hash]["state"];
        state.latest_credit_hash = credit_hash;
    }
    else if (mined_credit.relay_state_hash != 0)
        throw NoKnownRelayStateException(credit_hash);

    RelayStateChain state_chain(state);

    foreach_(const uint160 credit_hash_, credit_hashes)
    {
        log_ << credit_hash_ << "\n";
        state_chain.HandleBatch(credit_hash_);
    }

    if (state_chain.state.GetHash160() != final_state_hash)
    {
        uint160 original_credit_hash_ = original_credit_hash;
        
        while (original_credit_hash != 0)
        {
            MinedCredit mined_credit_ = creditdata[original_credit_hash]["mined_credit"];
            original_credit_hash = mined_credit_.previous_mined_credit_hash;
        }
        original_credit_hash = original_credit_hash_;
        while (original_credit_hash != 0)
        {
            MinedCredit mined_credit_ = creditdata[original_credit_hash]["mined_credit"];
            original_credit_hash = mined_credit_.previous_mined_credit_hash;
        }
    }

    return state_chain.state;
}

void RecordRelayState(RelayState state, uint160 credit_hash)
{
    relaydata[credit_hash]["state"] = state;
}



/****************
 *  RelayState
 */

    string_t RelayState::ToString() const
    {
        MinedCredit last_credit = creditdata[latest_credit_hash]["mined_credit"];
        uint32_t batch_number = last_credit.batch_number;
        std::stringstream ss;
        ss << "\n============== Relay State =============" << "\n"
        << "== Latest Credit Hash: "
        << latest_credit_hash.ToString()
        << " (" << batch_number << ")" << "\n"
        << "== Number of Relays: " << relays.size() << "\n"
        << "== Latest Relay Number: " << latest_relay_number << "\n"
        << "==" << "\n"
        << "== Relays:" << "\n";
                foreach_(Numbering::value_type item, relays)
                    {
                        uint32_t relay_number = item.second;
                        Point relay = item.first;
                        ss << "== " << relay_number << ": " << relay.ToString() << "\n";
                    }
        ss << "== " << "\n"
        << "== Hash: " << GetHash160().ToString() << "\n"
        << "============ End Relay State ===========" << "\n";
        return ss.str();
    }

    uint160 RelayState::GetHash160() const
    {
        if (relays.size() == 0 &&
            line_of_succession.size() == 0 &&
            subthreshold_succession_msgs.size() == 0 &&
            disqualifications.size() == 0 &&
            latest_credit_hash == 0)
            return 0;
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }

    uint32_t RelayState::AddRelay(Point relay, uint160 join_hash)
    {
        log_ << "RelayState::AddRelay: " << relay << "\n";

        if (relays.count(relay))
            return relays[relay];
        latest_relay_number += 1;
        log_ << "relaystate: latest number has been increased to "
        << latest_relay_number << "\n";

        relays[relay] = latest_relay_number;
        join_hash_to_relay[join_hash] = relay;
        TrimRelays();
        return latest_relay_number;
    }

    void RelayState::AddDisqualification(Point relay, uint160 message_hash)
    {
        if (!disqualifications.count(relay))
            disqualifications[relay] = std::set<uint160>();

        disqualifications[relay].insert(message_hash);
    }

    void RelayState::RemoveRelay(Point relay, bool decrement=true)
    {
        if (!relays.count(relay))
        {
            log_ << "RemoveRelay(): state is " << ToString();
            throw RelayStateError("No such relay to remove: "
                                  + relay.ToString());
        }

        relays.erase(relay);
        uint160 join_hash = GetJoinHash(relay);
        join_hash_to_relay.erase(join_hash);

        line_of_succession.erase(relay);
        actual_successions.erase(relay);
        subthreshold_succession_msgs.erase(relay);
        disqualifications.erase(relay);
        RemoveRelayFromSuccessors(relay);
        if (decrement)
            latest_relay_number--;
    }

    void RelayState::RemoveRelayFromSuccessors(Point relay)
    {
        std::vector<Point> to_delete;

                foreach_(const PointMap::value_type& item, line_of_succession)
                        if (item.second == relay)
                            to_delete.push_back(item.first);

                foreach_(const Point& predecessor, to_delete)
                    {
                        line_of_succession.erase(predecessor);
                        actual_successions.erase(predecessor);
                    }
    }

    void RelayState::TrimRelays()
    {
        while (relays.size() > MAX_RELAYS_REMEMBERED)
        {
            Point oldest_relay = OldestRelay();
            RemoveRelay(oldest_relay, false);
        }
    }

    Point RelayState::Relay(uint32_t relay_number)
    {
                foreach_(const Numbering::value_type& item, relays)
                    {
                        if (item.second == relay_number)
                            return item.first;
                    }
        return Point(SECP256K1);
    }

    uint160 RelayState::GetJoinHash(Point relay)
    {
                foreach_(const HashToPointMap::value_type& item, join_hash_to_relay)
                    {
                        if (item.second == relay)
                            return item.first;
                    }
        return 0;
    }

    Point RelayState::OldestRelay()
    {
        uint32_t minimum = 1000000000;

                foreach_(const Numbering::value_type& item, relays)
                    {
                        if (item.second < minimum)
                            minimum = item.second;
                    }
        return Relay(minimum);
    }

    std::map<uint32_t, Point> RelayState::RelaysByNumber()
    {
        std::map<uint32_t, Point> relays_by_number;
                foreach_(const Numbering::value_type& item, relays)
                    {
                        relays_by_number[item.second] = item.first;
                    }
        return relays_by_number;
    }

    std::vector<Point> RelayState::Executors(Point relay)
    {
        uint32_t relay_number = relays[relay];
        return Executors(relay_number);
    }

    Point RelayState::Successor(uint32_t relay_number)
    {
        std::map<uint32_t, Point> relays_by_number = RelaysByNumber();

        uint32_t successor_number;
        if (relay_number % FUTURE_SUCCESSOR_FREQUENCY != 1)
            successor_number = relay_number - 1;
        else
            successor_number = relay_number + 2 * FUTURE_SUCCESSOR_FREQUENCY - 1;

        if (successor_number > latest_relay_number)
        {
            return Point(SECP256K1, 0);
        }
        return relays_by_number[successor_number];
    }

    Point RelayState::Successor(Point relay)
    {
        std::map<uint32_t, Point> relays_by_number = RelaysByNumber();
        return Successor(relays[relay]);
    }

    Point RelayState::DeadPredecessor(Point relay)
    {
                foreach_(PointMap::value_type& item, actual_successions)
                    {
                        if (relay == item.second)
                            return item.first;
                    }
        return Point(SECP256K1, 0);
    }

    Point RelayState::SurvivingSuccessor(Point relay)
    {
        Point successor = Successor(relay);
        while (actual_successions.count(successor))
            successor = Successor(successor);
        return successor;
    }

    std::vector<Point> RelayState::GetEligibleRelays()
    {
        std::vector<Point> eligible_relays;
                foreach_(const Numbering::value_type& item, relays)
                    {
                        Point relay = item.first;

                        if (!actual_successions.count(relay) &&
                            !disqualifications.count(relay))
                            eligible_relays.push_back(relay);
                    }
        return eligible_relays;
    }

    Point RelayState::SelectRelay(uint64_t chooser_number)
    {
        if (NumberOfValidRelays() == 0)
            return Point(SECP256K1);

        if (latest_relay_number > MAX_POOL_SIZE)
            chooser_number = latest_relay_number - MAX_POOL_SIZE
                             + (chooser_number % MAX_POOL_SIZE);
        else
            chooser_number = 1 + (chooser_number % latest_relay_number);

        std::map<uint32_t, Point> relays_by_number = RelaysByNumber();

        Point relay = relays_by_number[chooser_number];

        while (actual_successions.count(relay)
               || disqualifications.count(relay))
        {
            relay = Successor(relay);
            if (!relay)
                relay = relays_by_number[latest_relay_number];
        }
        return relay;
    }

    uint32_t RelayState::NumberOfValidRelays()
    {
        uint32_t num = 0;
                foreach_(const Numbering::value_type& item, relays)
                    {
                        Point relay = item.first;
                        if (!actual_successions.count(relay) &&
                            !disqualifications.count(relay))
                            num += 1;
                    }
        return num;
    }

    void RelayState::SetLatestCreditHash(uint160 credit_hash)
    {
        latest_credit_hash = credit_hash;
    }

    std::vector<Point> RelayState::Executors(uint32_t relay_number)
    {
        std::vector<Point> executors;
        std::map<uint32_t, Point> relays_by_number = RelaysByNumber();

        foreach_(const uint32_t &offset, executor_offsets)
        {
            if (offset >= relay_number)
                continue;
            
            uint32_t executor_number = relay_number - offset;
            Point executor_key = relays_by_number[executor_number];
            executors.push_back(executor_key);
        }
        return executors;
    }

    std::vector<Point> RelayState::ChooseRelays(uint160 chooser,
                                                uint32_t num_relays_required)
    {
        std::vector<Point> chosen_relays;
        std::vector<Point> eligible_relays = GetEligibleRelays();

        uint64_t num_valid_relays = eligible_relays.size();

        if (num_relays_required > num_valid_relays)
        {
            log_ << "ChooseRelays: Not enough relays available for trade:"
                 << num_relays_required << " vs " << num_valid_relays << "\n"
                 << ToString();
            return chosen_relays;
        }

        while (eligible_relays.size() > MAX_POOL_SIZE)
        {
            EraseEntryFromVector(eligible_relays[0], eligible_relays);
        }

        while (chosen_relays.size() < num_relays_required &&
               chosen_relays.size() < num_valid_relays &&
               eligible_relays.size() > 0)
        {
            chooser = HashUint160(chooser);
            CBigNum chooser_ = CBigNum(chooser);
         
            uint64_t chooser_number = chooser_.getuint() 
                                        % eligible_relays.size();
            
            Point relay_pubkey = eligible_relays[chooser_number];
            
            if (relay_pubkey.IsAtInfinity())
                 log_ << "ChooseRelays(): selected pubkey was infinity!\n";
            if (!relay_pubkey.IsAtInfinity() && 
                !VectorContainsEntry(chosen_relays, relay_pubkey))
            {
                chosen_relays.push_back(relay_pubkey);
                EraseEntryFromVector(relay_pubkey, eligible_relays);
            }
        }
        return chosen_relays;
    }

/*
 *  RelayState
 ****************/

/*****************
 *  SharedChain
 */

    template<typename S>
    void SharedChain<S>::HandleBatch(uint160 credit_hash)
    {
        MinedCreditMessage msg = creditdata[credit_hash]["msg"];
        if (!msg.hash_list.RecoverFullHashes())
        {
            throw RecoverHashesFailedException(credit_hash);
        }
        foreach_(const uint160& message_hash, msg.hash_list.full_hashes)
        {
            string_t type = msgdata[message_hash]["type"];
            if (VectorContainsEntry(message_types, type))
            {
                HandleMessage(message_hash);
            }
        }
        state.latest_credit_hash = credit_hash;
    }

/*
 *  SharedChain
 *****************/

/********************
 *  RelayStateChain
 */

    bool RelayStateChain::ValidateBatch(uint160 credit_hash)
    {
        MinedCreditMessage msg = creditdata[credit_hash]["msg"];

        if (!msg.hash_list.RecoverFullHashes())
        {
            throw MissingDataException("recover hashes failed: "
                                       + credit_hash.ToString());
        }
        LOCK(relay_handler_mutex);
        RelayStateChain existing_state_chain = (*this);

        RelayState state_
            = GetRelayState(msg.mined_credit.previous_mined_credit_hash);

        (*this) = RelayStateChain(state_);
        bool result = HandleMessages(msg.hash_list.full_hashes);
        if (!result)
        {
            log_ << "RelayStateChain::ValidateBatch FAILED\n";
            log_ << "calendar is " << teleportnode.calendar;
        }

        (*this) = existing_state_chain;
        return result;
    }

    bool RelayStateChain::HandleMessages(vector<uint160> messages)
    {
        vector<uint160> rejected;
        return HandleMessages(messages, rejected);
    }

    bool RelayStateChain::HandleMessages(vector<uint160> messages,
                                         vector<uint160>& rejected)
    {
        bool result = true;
        vector<RelayJoinMessage> joins;
        vector<uint160> messages_to_handle;

        foreach_(const uint160& message_hash, messages)
        {
            string_t type = msgdata[message_hash]["type"];
            if (VectorContainsEntry(message_types, type))
            {
                if (type == "join")
                {
                    RelayJoinMessage join = msgdata[message_hash]["join"];
                    joins.push_back(join);
                }
                else
                {
                    messages_to_handle.push_back(message_hash);
                }
            }
        }
        sort(joins.begin(), joins.end());
        vector<uint160> final_messages_to_handle;

        foreach_(RelayJoinMessage join, joins)
            final_messages_to_handle.push_back(join.GetHash160());

        foreach_(uint160 hash, messages_to_handle)
            final_messages_to_handle.push_back(hash);

        foreach_(uint160 message_hash, final_messages_to_handle)
        {
            string_t type = msgdata[message_hash]["type"];
            
            if (!ValidateMessage(message_hash))
            {
                log_ << "RelayStateChain::HandleMessages: "
                     << "failed to validate " << message_hash << "\n";
                rejected.push_back(message_hash);
                result = false;
            }
            else
            {
                HandleMessage(message_hash);
            }
        }
        return result;
    }


    bool RelayStateChain::ValidateJoin(uint160 message_hash)
    {
        LOCK(relay_handler_mutex);
        log_ << "RelayStateChain::ValidateJoin: " << message_hash << "\n";
        if (!msgdata[message_hash].HasProperty("join"))
        {
             log_ << "RelayStateChain::ValidateJoin - no join message "
                  << message_hash << "\n";
            return false;
        }
        
        RelayJoinMessage msg = msgdata[message_hash]["join"];

        if (msg.relay_number <= state.latest_relay_number)
        {
            log_ << "position " << msg.relay_number << " already occupied\n";
            return false;
        }

        if (msg.relay_number != 1 + state.latest_relay_number)
        {
            log_ << "ValidateJoin(): Attempt to add relays in wrong order: "
                 << msg.relay_number << " after " 
                 << state.latest_relay_number << "\n";
            return false;
        }

        Point relay = msg.relay_pubkey;

        if (state.join_hash_to_relay.count(message_hash) ||
            state.relays.count(relay))
        {
            log_ << "this relay has already joined\n";
            return false;
        }
        log_ << "ValidateJoin: " << message_hash << " is ok\n";
        return true;
    }

    bool RelayStateChain::ValidateLeave(uint160 message_hash)
    {
        LOCK(relay_handler_mutex);
        log_ << "RelayStateChain::ValidateLeave: " << message_hash << "\n";
        if (!msgdata[message_hash].HasProperty("leave"))
        {
            log_ << "RelayStateChain::ValidateLeave - no join message "
                 << message_hash << "\n";
            return false;
        }
        
        RelayLeaveMessage msg = msgdata[message_hash]["leave"];

        Point relay = msg.VerificationKey();
        if (state.actual_successions.count(relay)
            || !state.relays.count(relay))
        {
            log_ << "Relay not joined or already left\n";
            return false;
        }
        return true;
    }

    bool RelayStateChain::ValidateSuccessor(uint160 message_hash)
    {
        LOCK(relay_handler_mutex);
        log_ << "RelayStateChain::ValidateSuccessor: " << message_hash << "\n";

        if (!msgdata[message_hash].HasProperty("successor"))
        {
            throw MissingDataException("successor: " + message_hash.ToString());
        }
        FutureSuccessorSecretMessage msg = msgdata[message_hash]["successor"];
        Point predecessor = msg.VerificationKey();
        if (!state.relays.count(predecessor))
        {
            log_ << "predecessor has not joined\n";
            return false;
        }
        if (state.line_of_succession.count(predecessor))
        {
            log_ << predecessor << " already has a successor - "
                 << state.line_of_succession[predecessor] << "\n";
        }
        RelayJoinMessage join = msgdata[msg.successor_join_hash]["join"];
        Point successor = join.relay_pubkey;
        if (!state.relays.count(successor))
        {
            log_ << "successor has not joined\n";
            return false;
        }
        return true;
    }

    bool RelayStateChain::ValidateSuccession(uint160 message_hash)
    {
        LOCK(relay_handler_mutex);

        log_ << "RelayStateChain::ValidateSuccession: " << message_hash << "\n";

        if (!msgdata[message_hash].HasProperty("succession"))
        {
            throw MissingDataException("succession: " 
                                       + message_hash.ToString());
        }
        SuccessionMessage msg = msgdata[message_hash]["succession"];

        Point dead_relay = msg.Deceased();
        Point successor = msg.Successor();

        if (!state.relays.count(dead_relay))
        {
            log_ << dead_relay << " has not joined\n";
            return false;
        }
        if (!state.line_of_succession.count(dead_relay) ||
            state.line_of_succession[dead_relay] != successor)
        {
            log_ << "wrong successor!\n";
            return false;
        }
        if (!VectorContainsEntry(state.Executors(dead_relay), msg.executor))
        {
            log_ << msg.executor << " is not an executor for "
                 << dead_relay << "\n";
            return false;
        }
        return true;
    }

    bool RelayStateChain::ValidateTraderComplaint(uint160 message_hash)
    {
        log_ << "RelayStateChain::ValidateTraderComplaint: "
             << message_hash << "\n";
        TraderComplaint complaint = msgdata[message_hash]["trader_complaint"];

        return complaint.VerifySignature();
    }

    bool RelayStateChain::ValidateRefutationOfTraderComplaint(uint160 
                                                              message_hash)
    {
        log_ << "RelayStateChain::ValidateComplaintRefutation: "
             << message_hash << "\n";
        RefutationOfTraderComplaint refutation
            = msgdata[message_hash]["trader_complaint_refutation"];
        return refutation.Validate();
    }

    bool RelayStateChain::ValidateRefutationOfRelayComplaint(uint160 
                                                              message_hash)
    {
        log_ << "RelayStateChain::ValidateComplaintRefutation:"
             << message_hash << "\n";
        RefutationOfTraderComplaint refutation
            = msgdata[message_hash]["trader_complaint_refutation"];
        return refutation.Validate();
    }

    void RelayStateChain::HandleJoin(uint160 message_hash)
    {
        log_ << "RelayStateChain::HandleJoin()\n";
        if (!msgdata[message_hash].HasProperty("join"))
            throw MissingDataException("join: " + message_hash.ToString());

        LOCK(relay_handler_mutex);

        RelayJoinMessage msg = msgdata[message_hash]["join"];
                
        if (state.relays.count(msg.relay_number))
        {
            log_ << "RelayStateChain::HandleJoin: there already is a relay "
                 << msg.relay_number << "\n";
            return;
        }
        log_ << "HandleJoin: number is " << msg.relay_number << "\n";
        
        if (msg.relay_number != 1 + state.latest_relay_number)
        {
            throw RelayStateError("Attempt to add relays in wrong order: "
                                  + to_string(msg.relay_number) + " after "
                                  + to_string(state.latest_relay_number));
        }
        Point relay = msg.relay_pubkey;
        
        state.AddRelay(relay, message_hash);
        if (msg.preceding_relay_join_hash > 0)
        {
            RelayJoinMessage successor_join 
                = msgdata[msg.preceding_relay_join_hash]["join"];
            state.line_of_succession[relay] = successor_join.relay_pubkey;
        }
    }

    void RelayStateChain::UnHandleJoin(uint160 message_hash)
    {
        LOCK(relay_handler_mutex);
        RelayJoinMessage join = msgdata[message_hash]["join"];
        state.RemoveRelay(join.relay_pubkey);
    }

    void RelayStateChain::HandleLeave(uint160 message_hash)
    {
        RelayLeaveMessage msg = msgdata[message_hash]["leave"];

        Point predecessor = msg.VerificationKey();
        state.actual_successions[predecessor] = msg.successor;
    }

    void RelayStateChain::UnHandleLeave(uint160 message_hash)
    {
        RelayLeaveMessage msg = msgdata[message_hash]["leave"];        
        Point predecessor = msg.VerificationKey();
        state.actual_successions.erase(predecessor);
    }

    void RelayStateChain::AddDisqualification(Point relay, 
                                              uint160 message_hash)
    {
        if (!state.disqualifications.count(relay))
            state.disqualifications[relay] = set<uint160>();

        state.disqualifications[relay].insert(message_hash);
    }

    void RelayStateChain::RemoveDisqualification(Point relay,
                                                 uint160 message_hash)
    {
        if (!state.disqualifications.count(relay))
            return;
        state.disqualifications[relay].erase(message_hash);
        if (state.disqualifications[relay].size() == 0)
            state.disqualifications.erase(relay);
    }

    void RelayStateChain::HandleComplaintFromFutureSuccessor(
        uint160 message_hash)
    {
        ComplaintFromFutureSuccessor complaint
            = msgdata[message_hash]["complaint_future_successor"];
        RelayJoinMessage join = msgdata[complaint.JoinHash()]["join"];
        Point relay = join.relay_pubkey;
        AddDisqualification(relay, message_hash);
    }

    void RelayStateChain::UnHandleComplaintFromFutureSuccessor(
        uint160 message_hash)
    {
        ComplaintFromFutureSuccessor complaint
            = msgdata[message_hash]["complaint_future_successor"];
        RelayJoinMessage join = msgdata[complaint.JoinHash()]["join"];
        Point relay = join.VerificationKey();
        RemoveDisqualification(relay, message_hash);
    }

    void RelayStateChain::HandleRefutationOfComplaintFromFutureSuccessor(
        uint160 message_hash)
    {
        RefutationOfComplaintFromFutureSuccessor refutation
            = msgdata[message_hash]["future_successor_refutation"];
        ComplaintFromFutureSuccessor complaint
            = msgdata[refutation.complaint_hash]["complaint_future_successor"];
        RelayJoinMessage join = msgdata[complaint.JoinHash()]["join"];
        Point relay = join.relay_pubkey;
        RemoveDisqualification(relay, refutation.complaint_hash);
        AddDisqualification(complaint.VerificationKey(), message_hash);
    }

    void RelayStateChain::UnHandleRefutationOfComplaintFromFutureSuccessor(
        uint160 message_hash)
    {
        RefutationOfComplaintFromFutureSuccessor refutation
            = msgdata[message_hash]["future_successor_refutation"];
        ComplaintFromFutureSuccessor complaint
            = msgdata[refutation.complaint_hash]["complaint_future_successor"];
        RelayJoinMessage join = msgdata[complaint.JoinHash()]["join"];
        Point relay = join.relay_pubkey;
        AddDisqualification(relay, refutation.complaint_hash);
        RemoveDisqualification(complaint.VerificationKey(), message_hash);
    }

    void RelayStateChain::HandleSuccessor(uint160 message_hash)
    {
        FutureSuccessorSecretMessage msg = msgdata[message_hash]["successor"];
        Point predecessor = msg.VerificationKey();
        RelayJoinMessage join = msgdata[msg.successor_join_hash]["join"];
        Point successor = join.relay_pubkey;
        state.line_of_succession[predecessor] = successor;
    }

    void RelayStateChain::UnHandleSuccessor(uint160 message_hash)
    {
        FutureSuccessorSecretMessage msg = msgdata[message_hash]["successor"];
        Point predecessor = msg.VerificationKey();
        state.line_of_succession.erase(predecessor);
    }

    void RelayStateChain::HandleSuccession(uint160 message_hash)
    {
        SuccessionMessage msg = msgdata[message_hash]["succession"];
        Point predecessor = msg.Deceased();
        
        set<uint160> msgs = state.subthreshold_succession_msgs[predecessor];

        msgs.insert(message_hash);
        
        state.subthreshold_succession_msgs[predecessor] = msgs;

        if (msgs.size() >= RelaysRequired(NUM_EXECUTORS))
        {
            state.actual_successions[predecessor] = msg.Successor();
        }
        relaydata[predecessor]["succession_msgs"] = msgs;
    }

    void RelayStateChain::UnHandleSuccession(uint160 message_hash)
    {
        SuccessionMessage msg = msgdata[message_hash]["succession"];
        Point predecessor = msg.Deceased();
        
        std::vector<uint160> msgs = relaydata[predecessor]["succession_msgs"];
        EraseEntryFromVector(message_hash, msgs);

        if (msgs.size() < RelaysRequired(NUM_EXECUTORS))
        {
            state.actual_successions.erase(predecessor);
            std::set<uint160> succession_msgs = relaydata[predecessor]["succession_msgs"];
            state.subthreshold_succession_msgs[predecessor] = succession_msgs;
        }
        state.subthreshold_succession_msgs[predecessor].erase(message_hash);
    }

    void RelayStateChain::HandleTraderComplaint(uint160 message_hash)
    {
        TraderComplaint complaint = msgdata[message_hash]["trader_complaint"];
        TradeSecretMessage msg = msgdata[complaint.message_hash]["secret"];

        Point relay = msg.VerificationKey();
        AddDisqualification(relay, message_hash);
    }

    void RelayStateChain::UnHandleTraderComplaint(uint160 message_hash)
    {
        TraderComplaint complaint = msgdata[message_hash]["complaint"];
        TradeSecretMessage msg = msgdata[complaint.message_hash]["secret"];

        Point relay = msg.VerificationKey();
        RemoveDisqualification(relay, message_hash);
    }

    void RelayStateChain::HandleRefutationOfTraderComplaint(uint160 message_hash)
    {
        RefutationOfTraderComplaint refutation
            = msgdata[message_hash]["trader_complaint_refutation"];
        uint160 complaint_hash = refutation.complaint_hash;

        TraderComplaint complaint = msgdata[complaint_hash]["trader_complaint"];
        TradeSecretMessage msg = msgdata[complaint.message_hash]["secret"];

        Point relay = msg.VerificationKey();
        RemoveDisqualification(relay, complaint_hash);
    }

    void RelayStateChain::UnHandleRefutationOfTraderComplaint(uint160 message_hash)
    {
        RefutationOfTraderComplaint refutation
            = msgdata[message_hash]["trader_complaint_refutation"];
        uint160 complaint_hash = refutation.complaint_hash;

        TraderComplaint complaint = msgdata[complaint_hash]["complaint"];
        TradeSecretMessage msg = msgdata[complaint.message_hash]["secret"];

        Point relay = msg.VerificationKey();
        AddDisqualification(relay, complaint_hash);
    }

    void RelayStateChain::HandleRefutationOfRelayComplaint(uint160 message_hash)
    {
        RefutationOfRelayComplaint refutation
            = msgdata[message_hash]["relay_complaint_refutation"];
        RelayComplaint complaint
            = msgdata[refutation.complaint_hash]["relay_complaint"];
        
        Point relay = complaint.VerificationKey();
        AddDisqualification(relay, message_hash);
    }

    void RelayStateChain::UnHandleRefutationOfRelayComplaint(uint160 message_hash)
    {
        RefutationOfRelayComplaint refutation
            = msgdata[message_hash]["relay_complaint_refutation"];
        uint160 complaint_hash = refutation.complaint_hash;

        RelayComplaint complaint = msgdata[complaint_hash]["relay_complaint"];
        
        Point relay = complaint.VerificationKey();
        RemoveDisqualification(relay, message_hash);
    }

/*
 *  RelayStateChain
 ********************/
