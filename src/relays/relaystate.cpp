// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "relays/relaystate.h"
#include "relays/relays.h"
#include "credits/creditsign.h"
#include "flexnode/flexnode.h"

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
    log_ << "looking backwards, got to " << credit_hash << "\n";

    RelayState state;

    if (relaydata[credit_hash].HasProperty("state"))
    {
        state = relaydata[credit_hash]["state"];
        log_ << "got relay state: " << state;
        state.latest_credit_hash = credit_hash;
    }
    else if (mined_credit.relay_state_hash != 0)
        throw NoKnownRelayStateException(credit_hash);


    RelayStateChain state_chain(state);

    log_ << "proceeding forward through:\n"; 

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

        uint64_t num_valid_relays = NumberOfValidRelays();

        if (num_relays_required > num_valid_relays)
        {
            log_ << "ChooseRelays: Not enough relays available for trade:"
                 << num_relays_required << " vs " << num_valid_relays << "\n"
                 << ToString();
            return chosen_relays;
        }

        while (chosen_relays.size() < num_relays_required &&
               chosen_relays.size() < num_valid_relays)
        {
            chooser = HashUint160(chooser);
            CBigNum chooser_ = CBigNum(chooser);
         
            uint64_t relays_available = latest_relay_number < MAX_POOL_SIZE
                                  ? latest_relay_number : MAX_POOL_SIZE;
            CBigNum num_available = CBigNum(relays_available);
            uint64_t chooser_number
                = (chooser_ % relays_available).getuint();
            
            Point relay_pubkey = SelectRelay(chooser_number);
            
            if (relay_pubkey.IsAtInfinity())
                 log_ << "ChooseRelays(): selected pubkey was infinity!\n";
            if (!relay_pubkey.IsAtInfinity() && 
                !VectorContainsEntry(chosen_relays, relay_pubkey))
            {
                chosen_relays.push_back(relay_pubkey);
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
        log_ << "SharedChain::HandleBatch(): applying "
             << credit_hash << "\n";
        if (!msg.hash_list.RecoverFullHashes())
        {
            throw RecoverHashesFailedException(credit_hash);
        }
        foreach_(const uint160& message_hash, msg.hash_list.full_hashes)
        {
            log_ << "SharedChain::HandleBatch: next is "
                 << message_hash << "\n";
            string_t type = msgdata[message_hash]["type"];
            log_ << "type is " << type << "\n";
            if (VectorContainsEntry(message_types, type))
            {
                log_ << "in message types - handling\n";
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
        // log_ << "RelayStateChain::ValidateBatch(): state of existing chain: "
        //      << existing_state_chain.state << "\n";
        
        // log_ << "applying " << credit_hash << "\n";

        RelayState state_
            = GetRelayState(msg.mined_credit.previous_mined_credit_hash);

        (*this) = RelayStateChain(state_);

        // log_ << "to the state: " << state_;
        // log_ << "which is the state: " << state;

        // log_ << "state of existing chain now: "
        //      << existing_state_chain.state;

        // log_ << "RelayStateChain::ValidateBatch(): handling messages\n";
        bool result = HandleMessages(msg.hash_list.full_hashes);
        if (!result)
        {
            log_ << "RelayStateChain::ValidateBatch FAILED\n";
            log_ << "calendar is " << flexnode.calendar;
        }
        // log_ << "state before restoring original chain: " << state;
        (*this) = existing_state_chain;
        // log_ << "RelayStateChain::ValidateBatch(): state after restoring:"
        //      << state;
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
            log_ << "SharedChain::HandleMessages: next is "
                  << message_hash << "\n";
            string_t type = msgdata[message_hash]["type"];
            log_ << "type is " << type << "\n";
            
            if (!ValidateMessage(message_hash))
            {
                log_ << "RelayStateChain::HandleMessages: "
                     << "failed to validate " << message_hash << "\n";
                rejected.push_back(message_hash);
                result = false;
            }
            else
            {
                log_ << message_hash << " is ok; handling\n";
                HandleMessage(message_hash);
            }
        }
        return result;
    }


    bool RelayStateChain::ValidateJoin(uint160 message_hash)
    {
        LOCK(relay_handler_mutex);
        log_ << "RelayStateChain::ValidateJoin: " << message_hash << "\n";
        log_ << "ValidateJoin: relay state is " << state;
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
            state.subthreshold_succession_msgs[predecessor]
                = relaydata[predecessor]["succession_msgs"];
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
