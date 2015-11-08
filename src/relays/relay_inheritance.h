#ifndef FLEX_RELAY_INHERITANCE
#define FLEX_RELAY_INHERITANCE

#include "relays/relay_messages.h"


bool ValidateRelayLeaveMessage(RelayLeaveMessage msg);

void HandleRelayLeaveMessage(RelayLeaveMessage msg);

void LeaveRelayPool();

bool CheckSuccessionMessage(SuccessionMessage msg);

void DoSuccession(Point dead_relay);

void SendSuccessionSecrets(Point dead_relay);

uint32_t IncrementAndCountSuccessionSecrets(Point dead_relay);

void InheritDuties(RelayJoinMessage join_msg);

void ComplainAboutSuccessionMessageSecret(SuccessionMessage msg);

void ComplainAboutSuccessionMessageEncryption(SuccessionMessage msg);

void HandleInheritedTasks(Point predecessor);

void HandleInheritedTask(uint160 task_hash);

void HandleInheritedTransaction(uint160 task_hash);

void HandleInheritedSuccession(uint160 task_hash);


#endif