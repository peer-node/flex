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

class RelayMessageHandler : public MessageHandlerWithOrphanage
{
public:
    Data data;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    RelayState relay_state;
    RelayAdmissionHandler admission_handler;
    RelaySuccessionHandler succession_handler;

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

    void HandleRelayJoinMessage(RelayJoinMessage relay_join_message);

    void HandleKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void HandleKeyDistributionComplaint(KeyDistributionComplaint complaint);

    void HandleSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    void HandleSecretRecoveryComplaint(SecretRecoveryComplaint complaint);

    void HandleSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message);

    void HandleSecretRecoveryFailureMessageAfterDuration(uint160 failure_message_hash);

    void HandleRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message);

    void HandleGoodbyeMessage(GoodbyeMessage goodbye_message);

    void HandleGoodbyeComplaint(GoodbyeComplaint complaint);

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
};


#endif //TELEPORT_RELAYMESSAGEHANDLER_H
