#ifndef TELEPORT_RELAY
#define TELEPORT_RELAY

#include "trade/trade_messages.h"
#include "teleportnode/orphanage.h"
#include "teleportnode/schedule.h"
#include "credits/creditutil.h"


void DoScheduledSecretsReleaseCheck(uint160 accept_commit_hash);

void DoScheduledBackupSecretsReleaseCheck(uint160 accept_commit_hash);

void DoScheduledSecretsReleasedCheck(uint160 accept_commit_hash);

void RecordRelayTradeTasks(uint160 accept_commit_hash);

bool CheckNewRelayChoiceMessage(NewRelayChoiceMessage msg);

void HandleNewRelayChoiceMessage(NewRelayChoiceMessage msg);

void SendSecrets(uint160 accept_commit_hash, uint8_t direction);

void SendSecret(OrderCommit order_commit,
                AcceptCommit accept_commit,
                uint32_t position,
                uint8_t direction);

void SendTradeSecretMessage(uint160 order_commit_hash,
                            uint8_t direction,
                            CBigNum credit_secret,
                            CBigNum currency_secret,
                            Point credit_recipient,
                            Point currency_recipient,
                            uint32_t position);

void ScheduleCancellation(uint160 complaint_hash);

void DoScheduledCancellation(uint160 accept_commit_hash);

void DoScheduledTimeout(uint160 accept_commit_hash);

#endif