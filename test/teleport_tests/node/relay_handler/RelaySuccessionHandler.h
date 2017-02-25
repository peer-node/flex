#ifndef TELEPORT_RELAYSUCCESSIONHANDLER_H
#define TELEPORT_RELAYSUCCESSIONHANDLER_H


#include <test/teleport_tests/node/calendar/Calendar.h>
#include <src/teleportnode/schedule.h>
#include "GoodbyeComplaint.h"
#include "SecretRecoveryComplaint.h"
#include "RecoveryFailureAuditMessage.h"
#include "DurationWithoutResponseFromRelay.h"
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
    bool send_goodbye_complaints{true};
    bool send_succession_completed_messages{true};
    std::set<uint64_t> dead_relays;

    RelaySuccessionHandler(Data data, CreditSystem *credit_system, Calendar *calendar, RelayMessageHandler *relay_message_handler);

    void AddScheduledTasks()
    {
        scheduler.AddTask(ScheduledTask("obituary",
                                        &RelaySuccessionHandler::HandleObituaryAfterDuration, this));

        scheduler.AddTask(ScheduledTask("secret_recovery",
                                        &RelaySuccessionHandler::HandleSecretRecoveryMessageAfterDuration, this));

        scheduler.AddTask(ScheduledTask("secret_recovery_failure",
                                        &RelaySuccessionHandler::HandleSecretRecoveryFailureMessageAfterDuration, this));

        scheduler.AddTask(ScheduledTask("goodbye",
                                        &RelaySuccessionHandler::HandleGoodbyeMessageAfterDuration, this));
    }

    void HandleSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    void HandleSecretRecoveryComplaint(SecretRecoveryComplaint complaint);

    void HandleSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message);

    void HandleSecretRecoveryFailureMessageAfterDuration(uint160 failure_message_hash);

    void HandleRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message);

    void HandleGoodbyeMessage(GoodbyeMessage goodbye_message);

    void HandleGoodbyeMessageAfterDuration(uint160 goodbye_message_hash);

    void HandleGoodbyeComplaint(GoodbyeComplaint complaint);

    void HandleObituaryAfterDuration(uint160 obituary_hash);


    void SendSecretRecoveryMessages(Relay *dead_relay);

    void SendSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder);

    void StoreSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message, uint160 obituary_hash);

    SecretRecoveryMessage GenerateSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder);

    void ScheduleTaskToCheckWhichQuarterHoldersHaveRespondedToObituary(uint160 obituary_hash);

    void HandleRelayDeath(Relay *dead_relay);

    std::vector<uint64_t> GetKeyQuarterHoldersWhoHaventRespondedToObituary(uint160 obituary_hash);

    void ProcessKeyQuarterHoldersWhoHaventRespondedToObituary(uint160 obituary_hash);

    bool ValidateSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    bool TryToRetrieveSecretsFromSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    bool TryToRecoverSecretsFromSecretRecoveryMessages(std::vector<uint160> recovery_message_hashes);

    void SendSecretRecoveryFailureMessage(std::vector<uint160> recovery_message_hashes);

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

    bool TryToExtractSecretsFromGoodbyeMessage(GoodbyeMessage goodbye_message);

    bool ExtractSecretsFromGoodbyeMessage(GoodbyeMessage goodbye_message);

    void SendGoodbyeComplaint(GoodbyeMessage goodbye_message);

    void RecordResponseToMessage(uint160 response_hash, uint160 message_hash, std::string response_type);

    bool ValidateSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message);

    bool ValidateGoodbyeMessage(GoodbyeMessage &goodbye_message);

    bool ValidateSecretRecoveryComplaint(SecretRecoveryComplaint &complaint);

    bool ValidateRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message);

    bool ValidateGoodbyeComplaint(GoodbyeComplaint &complaint);

    SecretRecoveryFailureMessage GenerateSecretRecoveryFailureMessage(std::vector<uint160> recovery_message_hashes);

    void AcceptGoodbyeMessage(GoodbyeMessage &goodbye_message);

    void AcceptSecretRecoveryMessage(SecretRecoveryMessage &secret_recovery_message);

    void AcceptSecretRecoveryComplaint(SecretRecoveryComplaint &complaint);

    void AcceptSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message);

    void AcceptRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message);

    void AcceptGoodbyeComplaint(GoodbyeComplaint &complaint);

    void HandleSecretRecoveryMessageAfterDuration(uint160 recovery_message_hash);

    bool ValidateDurationWithoutResponseFromRelayAfterObituary(DurationWithoutResponseFromRelay &duration);

    bool ValidateDurationWithoutResponseAfterSecretRecoveryMessage(DurationWithoutResponse &duration);

    bool ValidateDurationWithoutResponseAfterGoodbyeMessage(DurationWithoutResponse &duration);

    bool ValidateDurationWithoutResponseFromRelayAfterSecretRecoveryFailureMessage(
            DurationWithoutResponseFromRelay &duration);

    void HandleTasksInheritedFromDeadRelay(SecretRecoveryMessage &recovery_message);

    void SendSuccessionCompletedMessage(GoodbyeMessage &goodbye_message);

    void HandleSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message);

    bool ValidateSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message);

    void SendSuccessionCompletedMessage(SecretRecoveryMessage &recovery_message);

    SuccessionCompletedMessage GenerateSuccessionCompletedMessage(SecretRecoveryMessage &recovery_message);

    bool AllFourSecretRecoveryMessagesHaveBeenReceived(SecretRecoveryMessage &secret_recovery_message);

    void HandleFourSecretRecoveryMessages(SecretRecoveryMessage &secret_recovery_message);
};


#endif //TELEPORT_RELAYSUCCESSIONHANDLER_H
