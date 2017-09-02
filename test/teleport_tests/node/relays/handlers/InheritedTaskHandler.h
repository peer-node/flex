#ifndef TELEPORT_INHERITEDTASKHANDLER_H
#define TELEPORT_INHERITEDTASKHANDLER_H


#include <test/teleport_tests/node/calendar/Calendar.h>
#include <test/teleport_tests/node/Data.h>
#include "test/teleport_tests/node/relays/structures/Relay.h"

class RelayMessageHandler;
class RelaySuccessionHandler;

class InheritedTaskHandler
{
public:
    Data data;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    RelayState *relay_state{NULL};
    RelayMessageHandler *relay_message_handler{NULL};
    RelaySuccessionHandler *succession_handler{NULL};

    InheritedTaskHandler(RelayMessageHandler *handler);

    void HandleInheritedTasks(Relay *successor);

    void HandleInheritedTask(Task task, Relay *successor);

    void HandleInheritedDeath(Task task, Relay *successor_quarter_holder);
};


#endif //TELEPORT_INHERITEDTASKHANDLER_H
