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
    for (auto task_hash : successor->tasks)
        HandleInheritedTask(task_hash, successor);
}

void InheritedTaskHandler::HandleInheritedTask(uint160 task_hash, Relay *successor)
{
    string message_type = data.MessageType(task_hash);

    if (message_type == "obituary")
        HandleInheritedObituary(task_hash, successor);
}

void InheritedTaskHandler::HandleInheritedObituary(uint160 obituary_hash, Relay *successor_quarter_holder)
{
    Obituary obituary = data.GetMessage(obituary_hash);
    auto dead_relay = relay_state->GetRelayByNumber(obituary.dead_relay_number);
    succession_handler->SendSecretRecoveryMessage(dead_relay, successor_quarter_holder);
}