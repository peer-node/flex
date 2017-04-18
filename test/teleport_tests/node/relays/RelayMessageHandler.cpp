#include "RelayMessageHandler.h"
#include "test/teleport_tests/node/credit_handler/MinedCreditMessageBuilder.h"

#include "log.h"
#define LOG_CATEGORY "RelayMessageHandler.cpp"

using std::vector;
using std::string;

void RelayMessageHandler::SetCreditSystem(CreditSystem *credit_system_)
{
    credit_system = credit_system_;
    admission_handler.credit_system = credit_system;
    succession_handler.credit_system = credit_system;
}

void RelayMessageHandler::SetCalendar(Calendar *calendar_)
{
    calendar = calendar_;
    admission_handler.calendar = calendar;
    succession_handler.calendar = calendar;
}

void RelayMessageHandler::SetMinedCreditMessageBuilder(MinedCreditMessageBuilder *builder_)
{
    builder = builder_;
}

void RelayMessageHandler::HandleRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    admission_handler.HandleRelayJoinMessage(relay_join_message);
}

bool RelayMessageHandler::ValidateRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    return admission_handler.ValidateRelayJoinMessage(relay_join_message);
}

void RelayMessageHandler::HandleKeyDistributionMessage(KeyDistributionMessage key_distribution_message)
{
    admission_handler.HandleKeyDistributionMessage(key_distribution_message);
}

bool RelayMessageHandler::ValidateKeyDistributionMessage(KeyDistributionMessage key_distribution_message)
{
    return admission_handler.ValidateKeyDistributionMessage(key_distribution_message);
}

void RelayMessageHandler::HandleKeyDistributionComplaint(KeyDistributionComplaint complaint)
{
    admission_handler.HandleKeyDistributionComplaint(complaint);
}

bool RelayMessageHandler::ValidateKeyDistributionComplaint(KeyDistributionComplaint complaint)
{
    return admission_handler.ValidateKeyDistributionComplaint(complaint);
}

void RelayMessageHandler::HandleNewlyDeadRelays()
{
    succession_handler.HandleNewlyDeadRelays();
}

void RelayMessageHandler::HandleSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message)
{
    succession_handler.HandleSecretRecoveryMessage(secret_recovery_message);
}

bool RelayMessageHandler::ValidateSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message)
{
    return succession_handler.ValidateSecretRecoveryMessage(secret_recovery_message);
}

void RelayMessageHandler::HandleSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message)
{
    succession_handler.HandleSecretRecoveryFailureMessage(failure_message);
}

bool RelayMessageHandler::ValidateSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message)
{
    return succession_handler.ValidateSecretRecoveryFailureMessage(failure_message);
}

void RelayMessageHandler::HandleSecretRecoveryComplaint(SecretRecoveryComplaint complaint)
{
    return succession_handler.HandleSecretRecoveryComplaint(complaint);
}

bool RelayMessageHandler::ValidateSecretRecoveryComplaint(SecretRecoveryComplaint &complaint)
{
    return succession_handler.ValidateSecretRecoveryComplaint(complaint);
}

void RelayMessageHandler::HandleRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message)
{
    succession_handler.HandleRecoveryFailureAuditMessage(audit_message);
}

bool RelayMessageHandler::ValidateRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message)
{
    return succession_handler.ValidateRecoveryFailureAuditMessage(audit_message);
}

void RelayMessageHandler::HandleGoodbyeMessage(GoodbyeMessage goodbye_message)
{
    succession_handler.HandleGoodbyeMessage(goodbye_message);
}

bool RelayMessageHandler::ValidateGoodbyeMessage(GoodbyeMessage &goodbye_message)
{
    return succession_handler.ValidateGoodbyeMessage(goodbye_message);
}

bool RelayMessageHandler::ValidateSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message)
{
    return succession_handler.ValidateSuccessionCompletedMessage(succession_completed_message);
}

bool RelayMessageHandler::ValidateDurationWithoutResponse(DurationWithoutResponse &duration)
{
    std::string message_type = data.MessageType(duration.message_hash);

    if (message_type == "key_distribution")
        return admission_handler.ValidateDurationWithoutResponseAfterKeyDistributionMessage(duration);

    if (message_type == "secret_recovery")
        return succession_handler.ValidateDurationWithoutResponseAfterSecretRecoveryMessage(duration);
    return true;
}

bool RelayMessageHandler::ValidateDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay &duration)
{
    std::string message_type = data.MessageType(duration.message_hash);

    if (message_type == "death")
        return succession_handler.ValidateDurationWithoutResponseFromRelayAfterDeath(duration);

    if (message_type == "secret_recovery_failure")
        return succession_handler.ValidateDurationWithoutResponseFromRelayAfterSecretRecoveryFailureMessage(duration);

    return true;
}

bool RelayMessageHandler::ValidateMessage(uint160 &message_hash)
{
    std::string message_type = data.MessageType(message_hash);

#define VALIDATE(classname)                                                     \
    if (message_type == classname::Type())                                      \
    {                                                                           \
        classname message = data.GetMessage(message_hash);                      \
        return Validate ## classname(message);                                  \
    }

    VALIDATE(RelayJoinMessage);
    VALIDATE(KeyDistributionMessage);
    VALIDATE(KeyDistributionComplaint);
    VALIDATE(SecretRecoveryMessage);
    VALIDATE(SecretRecoveryComplaint);
    VALIDATE(SecretRecoveryFailureMessage);
    VALIDATE(RecoveryFailureAuditMessage);
    VALIDATE(GoodbyeMessage);
    VALIDATE(SuccessionCompletedMessage);
    VALIDATE(DurationWithoutResponse);
    VALIDATE(DurationWithoutResponseFromRelay);

#undef VALIDATE

    return true;
}

void RelayMessageHandler::HandleDurationWithoutResponse(DurationWithoutResponse duration)
{
    relay_state.ProcessDurationWithoutResponse(duration, data);
}

void RelayMessageHandler::AcceptForEncodingInChainIfLive(uint160 message_hash)
{
    if (mode == LIVE and builder != NULL)
    {
        builder->AddToAcceptedMessages(message_hash);
        tip_handler.unencoded_observed_history.push_back(message_hash);
    }
}

void RelayMessageHandler::HandleDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay duration)
{
    relay_state.ProcessDurationWithoutResponseFromRelay(duration, data);
}

void RelayMessageHandler::HandleSuccessionCompletedMessage(SuccessionCompletedMessage succession_completed_message)
{
    succession_handler.HandleSuccessionCompletedMessage(succession_completed_message);
}

void RelayMessageHandler::HandleNewTip(MinedCreditMessage new_tip)
{
    tip_handler.HandleNewTip(new_tip);
}
