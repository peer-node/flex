#ifndef TELEPORT_RELAYSUCCESSIONHANDLER_H
#define TELEPORT_RELAYSUCCESSIONHANDLER_H


#include <test/teleport_tests/node/calendar/Calendar.h>
#include <src/teleportnode/schedule.h>
#include "test/teleport_tests/node/relays/messages/SecretRecoveryComplaint.h"
#include "test/teleport_tests/node/relays/messages/RecoveryFailureAuditMessage.h"
#include "test/teleport_tests/node/relays/structures/DurationWithoutResponseFromRelay.h"
#include "InheritedTaskHandler.h"

class RelayMessageHandler;

class RelaySuccessionHandler
{
public:
    Data data;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    RelayState *relay_state{NULL};
    RelayMessageHandler *relay_message_handler;
    InheritedTaskHandler inherited_task_handler;
    Scheduler scheduler;
    bool send_audit_messages{true};
    bool send_secret_recovery_messages{true};
    bool send_secret_recovery_complaints{true};
    bool send_succession_completed_messages{true};
    std::set<uint64_t> dead_relays;

    RelaySuccessionHandler(Data data, CreditSystem *credit_system, Calendar *calendar, RelayMessageHandler *relay_message_handler);

    void AddScheduledTasks()
    {
        scheduler.AddTask(ScheduledTask("death",
                                        &RelaySuccessionHandler::HandleDeathAfterDuration, this));

        scheduler.AddTask(ScheduledTask("secret_recovery",
                                        &RelaySuccessionHandler::HandleFourSecretRecoveryMessagesAfterDuration, this));

        scheduler.AddTask(ScheduledTask("secret_recovery_failure",
                                        &RelaySuccessionHandler::HandleSecretRecoveryFailureMessageAfterDuration, this));
    }

    void HandleSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    void HandleSecretRecoveryComplaint(SecretRecoveryComplaint complaint);

    void HandleSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message);

    void HandleSecretRecoveryFailureMessageAfterDuration(uint160 failure_message_hash);

    void HandleRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message);

    void HandleGoodbyeMessage(GoodbyeMessage goodbye_message);

    void HandleDeathAfterDuration(uint160 death);

    void SendSecretRecoveryMessages(Relay *dead_relay);

    void ScheduleTaskToCheckWhichQuarterHoldersHaveRespondedToDeath(uint160 death);

    void HandleRelayDeath(Relay *dead_relay);

    std::vector<uint64_t> GetKeyQuarterHoldersWhoHaventRespondedToDeath(uint160 death);

    void MarkKeyQuarterHoldersWhoHaventRespondedToDeathAsNotResponding(uint160 death);

    bool ValidateSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    bool TryToRetrieveSecretsFromSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    bool TryToRecoverSecretsFromSecretRecoveryMessages(std::array<uint160, 4> recovery_message_hashes);

    void SendSecretRecoveryFailureMessage(std::array<uint160, 4> recovery_message_hashes);

    bool RelaysPrivateSigningKeyIsAvailable(uint64_t relay_number);

    void StoreSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message);

    void ComplainIfThereAreBadEncryptedSecretsInSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    void SendSecretRecoveryComplaint(SecretRecoveryMessage recovery_message, uint32_t key_sharer_position,
                                     uint32_t key_part_position);

    SecretRecoveryComplaint
    GenerateSecretRecoveryComplaint(SecretRecoveryMessage recovery_message, uint32_t key_sharer_position,
                                    uint32_t key_part_position);

    void SendAuditMessagesInResponseToFailureMessage(SecretRecoveryFailureMessage failure_message);

    void SendAuditMessageFromQuarterHolderInResponseToFailureMessage(SecretRecoveryFailureMessage failure_message,
                                                                     SecretRecoveryMessage recovery_message);

    void StoreRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message);

    std::vector<uint64_t> RelaysNotRespondingToRecoveryFailureMessage(uint160 failure_message_hash);

    void HandleNewlyDeadRelays();

    void RecordResponseToMessage(uint160 response_hash, uint160 message_hash, std::string response_type);

    bool ValidateSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message);

    bool ValidateGoodbyeMessage(GoodbyeMessage &goodbye_message);

    bool ValidateSecretRecoveryComplaint(SecretRecoveryComplaint &complaint);

    bool ValidateRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message);

    SecretRecoveryFailureMessage GenerateSecretRecoveryFailureMessage(std::array<uint160, 4> recovery_message_hashes);

    void AcceptGoodbyeMessage(GoodbyeMessage &goodbye_message);

    void AcceptSecretRecoveryMessage(SecretRecoveryMessage &secret_recovery_message);

    void AcceptSecretRecoveryComplaint(SecretRecoveryComplaint &complaint);

    void AcceptSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message);

    void AcceptRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message);

    void HandleFourSecretRecoveryMessagesAfterDuration(uint160 recovery_message_hash);

    bool ValidateDurationWithoutResponseFromRelayAfterDeath(DurationWithoutResponseFromRelay &duration);

    bool ValidateDurationWithoutResponseAfterSecretRecoveryMessage(DurationWithoutResponse &duration);

    bool ValidateDurationWithoutResponseFromRelayAfterSecretRecoveryFailureMessage(
            DurationWithoutResponseFromRelay &duration);

    void HandleTasksInheritedFromDeadRelay(SecretRecoveryMessage &recovery_message);

    void HandleSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message);

    bool ValidateSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message);

    void SendSuccessionCompletedMessage(SecretRecoveryMessage &recovery_message);

    SuccessionCompletedMessage GenerateSuccessionCompletedMessage(SecretRecoveryMessage &recovery_message);

    bool AllFourSecretRecoveryMessagesHaveBeenReceived(SecretRecoveryMessage &secret_recovery_message);

    void HandleFourSecretRecoveryMessages(SecretRecoveryMessage &secret_recovery_message);

    bool ThereHasBeenAResponseToTheSecretRecoveryMessages(uint160 recovery_message_hash);

    bool RelayWasNonResponsiveAndHasNowResponded(uint160 death);

    SecretRecoveryMessage GenerateSecretRecoveryMessage(Relay *dead_relay, uint32_t position);

    void SendSecretRecoveryMessage(Relay *dead_relay, uint32_t position);
};


#endif //TELEPORT_RELAYSUCCESSIONHANDLER_H
