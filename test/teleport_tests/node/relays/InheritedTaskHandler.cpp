#include "InheritedTaskHandler.h"
#include "RelayMessageHandler.h"

using std::string;
using std::vector;

#include "log.h"
#define LOG_CATEGORY "InheritedTaskHandler.cpp"

InheritedTaskHandler::InheritedTaskHandler(RelayMessageHandler *relay_message_handler):
        data(relay_message_handler->data),
        credit_system(relay_message_handler->credit_system),
        calendar(relay_message_handler->calendar),
        relay_message_handler(relay_message_handler),
        relay_state(&relay_message_handler->relay_state),
        succession_handler(&relay_message_handler->succession_handler)
{ }

void InheritedTaskHandler::HandleInheritedTasks(Relay *successor)
{
    if (successor == NULL)
    {
        log_ << "InheritedTaskHandler::HandleInheritedTasks: successor is null\n";
        return;
    }
    for (auto task : successor->tasks)
        HandleInheritedTask(task, successor);
}

void InheritedTaskHandler::HandleInheritedTask(Task task, Relay *successor)
{
    if (IsDeath(task.event_hash))
        HandleInheritedDeath(task, successor);
}

void InheritedTaskHandler::HandleInheritedDeath(Task task, Relay *successor_quarter_holder)
{
    auto dead_relay = relay_state->GetRelayByNumber(GetDeadRelayNumber(task.event_hash));
    succession_handler->SendSecretRecoveryMessage(dead_relay, task.position);
}