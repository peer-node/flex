#ifndef TELEPORT_RELAYMESSAGEHANDLER_H
#define TELEPORT_RELAYMESSAGEHANDLER_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include <src/teleportnode/schedule.h>
#include "RelayState.h"
#include "CreditSystem.h"
#include "MessageHandlerWithOrphanage.h"
#include "RelayJoinMessage.h"
#include "KeyDistributionMessage.h"

#define RESPONSE_WAIT_TIME 8000000 // microseconds

class RelayMessageHandler : public MessageHandlerWithOrphanage
{
public:
    Data data;
    CreditSystem *credit_system;
    Calendar *calendar;
    RelayState relay_state;
    Scheduler scheduler;

    ScheduledTask handle_key_distribution_message_after_duration;
    ScheduledTask handle_obituary_after_duration;

    RelayMessageHandler(Data data):
            MessageHandlerWithOrphanage(data.msgdata), data(data.msgdata, data.creditdata, data.keydata, &relay_state)
    {
        channel = "relay";
    }

    void AddScheduledTasks()
    {
        handle_key_distribution_message_after_duration
                = ScheduledTask("key_distribution",
                                &RelayMessageHandler::HandleKeyDistributionMessageAfterDuration, this);
        scheduler.AddTask(handle_key_distribution_message_after_duration);

        handle_obituary_after_duration = ScheduledTask("obituary",
                                                       &RelayMessageHandler::HandleObituaryAfterDuration, this);
        scheduler.AddTask(handle_obituary_after_duration);
    }

    void SetCreditSystem(CreditSystem *credit_system);

    void SetCalendar(Calendar *calendar);

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(RelayJoinMessage);
        HANDLESTREAM(KeyDistributionMessage);
        HANDLESTREAM(KeyDistributionComplaint);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(RelayJoinMessage);
        HANDLEHASH(KeyDistributionMessage);
        HANDLEHASH(KeyDistributionComplaint);
    }

    HANDLECLASS(RelayJoinMessage);
    HANDLECLASS(KeyDistributionMessage);
    HANDLECLASS(KeyDistributionComplaint);

    void HandleRelayJoinMessage(RelayJoinMessage relay_join_message);

    void HandleKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void HandleKeyDistributionComplaint(KeyDistributionComplaint complaint);

    bool MinedCreditMessageHashIsInMainChain(RelayJoinMessage relay_join_message);

    void AcceptRelayJoinMessage(RelayJoinMessage join_message);

    bool ValidateRelayJoinMessage(RelayJoinMessage relay_join_message);

    bool MinedCreditMessageIsNewerOrMoreThanThreeBatchesOlderThanLatest(uint160 mined_credit_message_hash);

    bool ValidateKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void HandleKeyDistributionMessageAfterDuration(uint160 key_distribution_message_hash);

    void HandleObituaryAfterDuration(uint160 obituary_hash);

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

    void ScheduleTaskToCheckWhichQuarterHoldersHaveResponded(uint160 obituary_hash);

    void HandleRelayDeath(Relay *dead_relay);

    std::vector<uint64_t> GetKeyQuarterHoldersWhoHaventResponded(uint160 obituary_hash);
};


#endif //TELEPORT_RELAYMESSAGEHANDLER_H
