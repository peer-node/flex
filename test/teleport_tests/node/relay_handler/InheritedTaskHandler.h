#ifndef TELEPORT_INHERITEDTASKHANDLER_H
#define TELEPORT_INHERITEDTASKHANDLER_H


#include <test/teleport_tests/node/calendar/Calendar.h>
#include <test/teleport_tests/node/Data.h>
#include "Relay.h"

class RelayMessageHandler;
class RelaySuccessionHandler;

class InheritedTaskHandler
{
public:
    Data data;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    RelayState *relay_state{NULL};
    RelayMessageHandler *relay_message_handler;
    RelaySuccessionHandler *succession_handler;

    InheritedTaskHandler(RelayMessageHandler *handler);

    void HandleInheritedTasks(Relay *successor);

    void HandleInheritedTask(uint160 task_hash, Relay *successor);

    void HandleInheritedObituary(uint160 task_hash, Relay *successor);
};


#endif //TELEPORT_INHERITEDTASKHANDLER_H
