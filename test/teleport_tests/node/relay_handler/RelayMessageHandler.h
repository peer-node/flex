#ifndef TELEPORT_RELAYMESSAGEHANDLER_H
#define TELEPORT_RELAYMESSAGEHANDLER_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include <src/teleportnode/schedule.h>
#include "RelayState.h"
#include "test/teleport_tests/node/CreditSystem.h"
#include "test/teleport_tests/node/MessageHandlerWithOrphanage.h"
#include "RelayJoinMessage.h"
#include "KeyDistributionMessage.h"
#include "SecretRecoveryFailureMessage.h"
#include "RecoveryFailureAuditMessage.h"
#include "RelayAdmissionHandler.h"
#include "RelaySuccessionHandler.h"

#define RESPONSE_WAIT_TIME 8000000 // microseconds

#define LIVE 1
#define BLOCK_VALIDATION 2
#define INITIAL_DATA_VALIDATION 3

class MinedCreditMessageBuilder;

class RelayMessageHandler : public MessageHandlerWithOrphanage
{
public:
    Data data;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    MinedCreditMessageBuilder *builder{NULL};
    RelayState relay_state;
    RelayAdmissionHandler admission_handler;
    RelaySuccessionHandler succession_handler;

    uint8_t mode{LIVE};

    RelayMessageHandler(Data data):
            MessageHandlerWithOrphanage(data.msgdata),
            data(data.msgdata, data.creditdata, data.keydata, &relay_state),
            admission_handler(data, credit_system, calendar, this),
            succession_handler(data, credit_system, calendar, this)
    {
        channel = "relay";
    }

    void AddScheduledTasks()
    {
        admission_handler.AddScheduledTasks();
        succession_handler.AddScheduledTasks();
    }

    void SetCreditSystem(CreditSystem *credit_system);

    void SetCalendar(Calendar *calendar);

    void SetMinedCreditMessageBuilder(MinedCreditMessageBuilder *builder);

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
        HANDLESTREAM(GoodbyeComplaint);

        HANDLESTREAM(DurationWithoutResponse);
        HANDLESTREAM(DurationWithoutResponseFromRelay);
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
        HANDLEHASH(GoodbyeComplaint);

        HANDLEHASH(DurationWithoutResponse);
        HANDLEHASH(DurationWithoutResponseFromRelay);
    }

    HANDLECLASS(RelayJoinMessage);
    HANDLECLASS(KeyDistributionMessage);
    HANDLECLASS(KeyDistributionComplaint);
    HANDLECLASS(SecretRecoveryMessage);
    HANDLECLASS(SecretRecoveryComplaint);
    HANDLECLASS(SecretRecoveryFailureMessage);
    HANDLECLASS(RecoveryFailureAuditMessage);
    HANDLECLASS(GoodbyeMessage);
    HANDLECLASS(GoodbyeComplaint);

    HANDLECLASS(DurationWithoutResponse);
    HANDLECLASS(DurationWithoutResponseFromRelay);

    template <typename T>
    void Broadcast(T message)
    {
        if (mode != LIVE)
            throw std::runtime_error("attempt to broadcast through relay message handler while not live.");
        if (network != NULL)
            network->Broadcast(channel, message.Type(), message);
    }

    void HandleRelayJoinMessage(RelayJoinMessage relay_join_message);

    void HandleKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void HandleKeyDistributionComplaint(KeyDistributionComplaint complaint);

    void HandleSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    void HandleSecretRecoveryComplaint(SecretRecoveryComplaint complaint);

    void HandleSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message);

    void HandleRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message);

    void HandleGoodbyeMessage(GoodbyeMessage goodbye_message);

    void HandleGoodbyeComplaint(GoodbyeComplaint complaint);

    void HandleDurationWithoutResponse(DurationWithoutResponse duration);

    void HandleDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay duration);

    bool ValidateRelayJoinMessage(RelayJoinMessage relay_join_message);

    bool ValidateKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    bool ValidateKeyDistributionComplaint(KeyDistributionComplaint complaint);

    bool ValidateSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    void HandleNewlyDeadRelays();

    bool ValidateSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message);

    bool ValidateGoodbyeMessage(GoodbyeMessage &goodbye_message);

    bool ValidateSecretRecoveryComplaint(SecretRecoveryComplaint &complaint);

    bool ValidateRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message);

    bool ValidateGoodbyeComplaint(GoodbyeComplaint &complaint);

    bool ValidateMessage(uint160 &message_hash);

    bool ValidateDurationWithoutResponse(DurationWithoutResponse &duration);

    void EncodeInChainIfLive(uint160 message_hash);

    bool ValidateDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay &duration);
};


#endif //TELEPORT_RELAYMESSAGEHANDLER_H
