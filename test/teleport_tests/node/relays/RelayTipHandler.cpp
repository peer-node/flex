#include "RelayTipHandler.h"
#include "RelayMessageHandler.h"

using std::vector;

#include "log.h"
#define LOG_CATEGORY "RelayTipHandler.cpp"


RelayTipHandler::RelayTipHandler(RelayMessageHandler *relay_message_handler):
        relay_message_handler(relay_message_handler),
        relay_state(&relay_message_handler->relay_state),
        data(relay_message_handler->data)
{

}

void RelayTipHandler::HandleNewTip(MinedCreditMessage &new_tip)
{
    relay_state->latest_mined_credit_message_hash = new_tip.GetHash160();
    new_tip.hash_list.RecoverFullHashes(data.msgdata);
    for (auto message_hash : new_tip.hash_list.full_hashes)
    {
        relay_message_handler->HandleMessage(message_hash);
    }
}

void RelayTipHandler::RemoveEncodedEventsFromUnencodedObservedHistory(std::vector<uint160> encoded_events)
{
    for (auto encoded_event_hash : encoded_events)
        RemoveEncodedEventFromUnencodedObservedHistory(encoded_event_hash);
}

void RelayTipHandler::RemoveEncodedEventFromUnencodedObservedHistory(uint160 encoded_event_hash)
{
    for (auto observed_event_hash : unencoded_observed_history)
        if (EncodedEventMatchesObservedEvent(encoded_event_hash, observed_event_hash))
            return EraseEntryFromVector(observed_event_hash, unencoded_observed_history);
}

bool RelayTipHandler::EncodedEventMatchesObservedEvent(uint160 encoded_event_hash, uint160 observed_event_hash)
{
    if (encoded_event_hash == observed_event_hash)
        return true;;
    return false;
}