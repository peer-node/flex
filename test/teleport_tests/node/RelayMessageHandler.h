#ifndef TELEPORT_RELAYMESSAGEHANDLER_H
#define TELEPORT_RELAYMESSAGEHANDLER_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include <src/teleportnode/schedule.h>
#include "RelayState.h"
#include "CreditSystem.h"
#include "MessageHandlerWithOrphanage.h"
#include "RelayJoinMessage.h"
#include "KeyDistributionMessage.h"
#include "SecretRecoveryFailureMessage.h"
#include "RecoveryFailureAuditMessage.h"

#define RESPONSE_WAIT_TIME 8000000 // microseconds

class RelayMessageHandler : public MessageHandlerWithOrphanage
{
public:
    Data data;
    CreditSystem *credit_system;
    Calendar *calendar;
    RelayState relay_state;
    std::set<uint64_t> dead_relays;
    Scheduler scheduler;

    RelayMessageHandler(Data data):
            MessageHandlerWithOrphanage(data.msgdata), data(data.msgdata, data.creditdata, data.keydata, &relay_state)
    {
        channel = "relay";
    }

    void AddScheduledTasks()
    {
        scheduler.AddTask(ScheduledTask("key_distribution",
                                        &RelayMessageHandler::HandleKeyDistributionMessageAfterDuration, this));

        scheduler.AddTask(ScheduledTask("obituary",
                                        &RelayMessageHandler::HandleObituaryAfterDuration, this));

        scheduler.AddTask(ScheduledTask("secret_recovery_failure",
                                        &RelayMessageHandler::HandleSecretRecoveryFailureMessageAfterDuration, this));

        scheduler.AddTask(ScheduledTask("goodbye",
                                        &RelayMessageHandler::HandleGoodbyeMessageAfterDuration, this));
    }

    void SetCreditSystem(CreditSystem *credit_system);

    void SetCalendar(Calendar *calendar);

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(RelayJoinMessage);
        HANDLESTREAM(KeyDistributionMessage);
        HANDLESTREAM(KeyDistributionComplaint);
        HANDLESTREAM(SecretRecoveryMessage);
        HANDLESTREAM(SecretRecoveryComplaint);
        HANDLESTREAM(SecretRecoveryFailureMessage);
        HANDLESTREAM(RecoveryFailureAuditMessage);
        HANDLESTREAM(GoodbyeMessage);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(RelayJoinMessage);
        HANDLEHASH(KeyDistributionMessage);
        HANDLEHASH(KeyDistributionComplaint);
        HANDLEHASH(SecretRecoveryMessage);
        HANDLEHASH(SecretRecoveryComplaint);
        HANDLEHASH(SecretRecoveryFailureMessage);
        HANDLEHASH(RecoveryFailureAuditMessage);
        HANDLEHASH(GoodbyeMessage);
    }

    HANDLECLASS(RelayJoinMessage);
    HANDLECLASS(KeyDistributionMessage);
    HANDLECLASS(KeyDistributionComplaint);
    HANDLECLASS(SecretRecoveryMessage);
    HANDLECLASS(SecretRecoveryComplaint);
    HANDLECLASS(SecretRecoveryFailureMessage);
    HANDLECLASS(RecoveryFailureAuditMessage);
    HANDLECLASS(GoodbyeMessage);

    void HandleRelayJoinMessage(RelayJoinMessage relay_join_message);

    void HandleKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void HandleKeyDistributionMessageAfterDuration(uint160 key_distribution_message_hash);

    void HandleKeyDistributionComplaint(KeyDistributionComplaint complaint);

    void HandleSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    void HandleSecretRecoveryComplaint(SecretRecoveryComplaint complaint);

    void HandleSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message);

    void HandleSecretRecoveryFailureMessageAfterDuration(uint160 failure_message_hash);

    void HandleRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message);

    void HandleGoodbyeMessage(GoodbyeMessage goodbye_message);

    void HandleGoodbyeMessageAfterDuration(uint160 goodbye_message_hash);

    void HandleObituaryAfterDuration(uint160 obituary_hash);

    bool MinedCreditMessageHashIsInMainChain(RelayJoinMessage relay_join_message);

    void AcceptRelayJoinMessage(RelayJoinMessage join_message);

    bool ValidateRelayJoinMessage(RelayJoinMessage relay_join_message);

    bool MinedCreditMessageIsNewerOrMoreThanThreeBatchesOlderThanLatest(uint160 mined_credit_message_hash);

    bool ValidateKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void StoreKeyDistributionSecretsAndSendComplaints(KeyDistributionMessage key_distribution_message);

    void SendKeyDistributionComplaint(uint160 key_distribution_message_hash, uint64_t set_of_secrets,
                                      uint32_t position_of_bad_secret);

    void RecoverSecretsAndSendKeyDistributionComplaints(Relay *relay, uint64_t set_of_secrets,
                                                        std::vector<uint64_t> recipients,
                                                        uint160 key_distribution_message_hash,
                                                        std::vector<CBigNum> encrypted_secrets);

    bool RecoverAndStoreSecret(Relay *recipient, CBigNum &encrypted_secret, Point &point_corresponding_to_secret);

    void RecordSentComplaint(uint160 complaint_hash, uint160 bad_message_hash);

    bool ValidateKeyDistributionComplaint(KeyDistributionComplaint complaint);

    void SendSecretRecoveryMessages(Relay *dead_relay);

    void SendSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder);

    void StoreSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message, uint160 obituary_hash);

    SecretRecoveryMessage GenerateSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder);

    void ScheduleTaskToCheckWhichQuarterHoldersHaveRespondedToObituary(uint160 obituary_hash);

    void HandleRelayDeath(Relay *dead_relay);

    std::vector<uint64_t> GetKeyQuarterHoldersWhoHaventResponded(uint160 obituary_hash);

    void ProcessKeyQuarterHoldersWhoHaventRespondedToObituary(uint160 obituary_hash);

    bool ValidateSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    void TryToRetrieveSecretsFromSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

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

    void StoreSecretRecoveryComplaint(SecretRecoveryComplaint complaint);

    void SendAuditMessagesInResponseToFailureMessage(SecretRecoveryFailureMessage failure_message);

    void SendAuditMessageFromQuarterHolderInResponseToFailureMessage(SecretRecoveryFailureMessage failure_message,
                                                                     SecretRecoveryMessage recovery_message);

    void StoreRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message);

    std::vector<uint64_t> RelaysNotRespondingToRecoveryFailureMessage(uint160 failure_message_hash);

    void HandleNewlyDeadRelays();

    void TryToExtractSecretsFromGoodbyeMessage(GoodbyeMessage goodbye_message);

    bool ExtractSecretsFromGoodbyeMessage(GoodbyeMessage goodbye_message);

    void SendGoodbyeComplaint(GoodbyeMessage goodbye_message);
};


#endif //TELEPORT_RELAYMESSAGEHANDLER_H
