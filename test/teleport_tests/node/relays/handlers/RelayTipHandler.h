#ifndef TELEPORT_RELAYTIPHANDLER_H
#define TELEPORT_RELAYTIPHANDLER_H

#include <test/teleport_tests/node/Data.h>
#include <test/teleport_tests/node/credit_handler/messages/MinedCreditMessage.h>

class RelayMessageHandler;

class RelayTipHandler
{
public:
    Data data;
    RelayMessageHandler *relay_message_handler;
    RelayState *relay_state;
    std::vector<uint160> unencoded_observed_history;

    RelayTipHandler(RelayMessageHandler *relay_message_handler);

    void HandleNewTip(MinedCreditMessage &new_tip);

    void RemoveEncodedEventsFromUnencodedObservedHistory(std::vector<uint160> encoded_events);

    void RemoveEncodedEventFromUnencodedObservedHistory(uint160 encoded_event_hash);

    bool EncodedEventMatchesObservedEvent(uint160 encoded_event_hash, uint160 observed_event_hash);
};


#endif //TELEPORT_RELAYTIPHANDLER_H
