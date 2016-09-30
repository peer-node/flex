// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "teleportnode/teleportnode.h"

#include "log.h"
#define LOG_CATEGORY "relays.cpp"

CCriticalSection relay_handler_mutex;

template <typename T>
std::vector<T> TrimToLast(std::vector<T> entries, uint32_t n)
{
    uint32_t first_kept_entry = entries.size() - n;
    std::vector<T> kept(&entries[first_kept_entry], 
                        &entries[first_kept_entry + n]);
    return kept;
}

Point SelectRelayFromState(uint64_t chooser_number, uint160 credit_hash)
{
    MinedCredit mined_credit = creditdata[credit_hash]["mined_credit"];
    RelayState state = relaydata[credit_hash]["state"];
    
    return state.SelectRelay(chooser_number);
}


void InitializeRelayTasks()
{
    teleportnode.scheduler.AddTask(ScheduledTask("join_check", 
                                               DoScheduledJoinCheck));
    teleportnode.scheduler.AddTask(ScheduledTask("join_complaint", 
                                               DoScheduledJoinComplaintCheck));
    teleportnode.scheduler.AddTask(
        ScheduledTask("future_successor_complaint", 
                      DoScheduledFutureSuccessorComplaintCheck));
    teleportnode.scheduler.AddTask(
        ScheduledTask("succession_complaint", 
                      DoScheduledSuccessionComplaintCheck));
    teleportnode.scheduler.AddTask(ScheduledTask("succession_check", 
                                               DoScheduledSuccessionCheck));
}



/******************
 *  RelayHandler
 */

    void RelayHandler::AddBatchToTip(MinedCreditMessage& msg)
    {
        log_ << "RelayHandler::AddBatchToTip(): " << msg.GetHash160() << "\n";
            

        if (relay_chain.state.latest_credit_hash 
            != msg.mined_credit.previous_mined_credit_hash)
        {
            throw WrongBatchException("attempt to add "
                                      + msg.mined_credit.ToString() 
                                      + " after "
                                      + relay_chain.state.ToString());
        }
        
        std::vector<uint160> msgs_yet_to_be_encoded;
        msg.hash_list.RecoverFullHashes();

        LOCK(relay_handler_mutex);
        {
            foreach_(const uint160& msg_hash, relay_chain.handled_messages)
            {
                msgs_yet_to_be_encoded.push_back(msg_hash);
                log_ << "RelayHandler::AddBatchToTip(): unhandling "
                     << msg_hash << "\n";
                relay_chain.UnHandleMessage(msg_hash);
            }

            relay_chain.HandleMessages(msg.hash_list.full_hashes);

            foreach_(uint160 msg_hash, msg.hash_list.full_hashes)
                EraseEntryFromVector(msg_hash, msgs_yet_to_be_encoded);
            
            relay_chain.handled_messages = std::set<uint160>();
            
            log_ << "handling yet-to-be-encoded msgs\n";
            relay_chain.HandleMessages(msgs_yet_to_be_encoded);
            
            uint160 credit_hash = msg.mined_credit.GetHash160();

            relay_chain.state.latest_credit_hash = credit_hash;

            HandleQueuedJoins(credit_hash);
        }
        Point latest_relay
            = relay_chain.state.Relay(relay_chain.state.latest_relay_number);
        latest_valid_join_hash = relay_chain.state.GetJoinHash(latest_relay);
    }
    
    void RelayHandler::UnHandleBatch()
    {
        MinedCreditMessage msg
            = creditdata[relay_chain.state.latest_credit_hash]["msg"];
        UnHandleBatch(msg);
    }

    void RelayHandler::UnHandleBatch(MinedCreditMessage& msg)
    {
        LOCK(relay_handler_mutex);
        uint160 credit_hash = msg.mined_credit.GetHash160();
        log_ << "UnHandleBatch(): " << credit_hash << "\n";
        if (credit_hash != relay_chain.state.latest_credit_hash)
        {
            throw WrongBatchException("Can't UnHandle " 
                                      + credit_hash.ToString()
                                      + " when state is "
                                      + relay_chain.state.ToString());
        }

        vector<uint160> msgs_yet_to_be_encoded;
    
        msg.hash_list.RecoverFullHashes();
        foreach_(const uint160& msg_hash, msg.hash_list.full_hashes)
            msgs_yet_to_be_encoded.push_back(msg_hash);

        foreach_(const uint160& msg_hash, relay_chain.handled_messages)
        {
            msgs_yet_to_be_encoded.push_back(msg_hash);
            relay_chain.UnHandleMessage(msg_hash);
        }
        
        foreach_(const uint160& msg_hash, msg.hash_list.full_hashes)
        {   
            relay_chain.UnHandleMessage(msg_hash);
        }
        uint160 prev_hash = msg.mined_credit.previous_mined_credit_hash;
        relay_chain.state.latest_credit_hash = prev_hash;
        MinedCredit mined_credit = creditdata[prev_hash]["mined_credit"];

        RelayState state = GetRelayState(mined_credit.relay_state_hash);

        Point latest_relay = state.Relay(state.latest_relay_number);
        latest_valid_join_hash = state.GetJoinHash(latest_relay);

        foreach_(const uint160& msg_hash, msgs_yet_to_be_encoded)
        {
            log_ << "UnHandleBatch:" << msg_hash << " is yet to be encoded\n";
            if (relay_chain.ValidateMessage(msg_hash))
            {
                log_ << msg_hash << " is ok; handling\n";
                relay_chain.HandleMessage(msg_hash);
            }
        }
    
    }

    std::vector<Point> RelayHandler::Executors(uint32_t relay_number)
    {
        return relay_chain.state.Executors(relay_number);
    }
    
    std::vector<Point> RelayHandler::Executors(Point relay)
    {
        return relay_chain.state.Executors(relay);
    }
    
    Point RelayHandler::Successor(uint32_t relay_number)
    {
        return relay_chain.state.Successor(relay_number);
    }

    Point RelayHandler::Successor(Point relay)
    {
        return relay_chain.state.Successor(relay);
    }

/*
 *  RelayHandler
 ******************/

