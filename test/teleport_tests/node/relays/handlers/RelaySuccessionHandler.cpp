#include "RelaySuccessionHandler.h"
#include "RelayMessageHandler.h"
#include "test/teleport_tests/node/credit/handlers/MinedCreditMessageBuilder.h"


#include "log.h"
#define LOG_CATEGORY "RelaySuccessionHandler.cpp"

using std::vector;
using std::string;
using std::set;

RelaySuccessionHandler::RelaySuccessionHandler(Data data_, CreditSystem *credit_system, Calendar *calendar,
                                               RelayMessageHandler *relay_message_handler):
        data(data_), relay_state(&relay_message_handler->relay_state), credit_system(credit_system), calendar(calendar),
        relay_message_handler(relay_message_handler), inherited_task_handler(relay_message_handler)
{
    data.relay_state = relay_state;
}

void RelaySuccessionHandler::HandleNewlyDeadRelays()
{
    for (auto &relay : relay_state->relays)
        if (relay.status != ALIVE and dead_relays.count(relay.number) == 0)
            HandleRelayDeath(&relay);
}

void RelaySuccessionHandler::HandleRelayDeath(Relay *dead_relay)
{
    dead_relays.insert(dead_relay->number);
    if (relay_message_handler->mode == LIVE)
    {
        if (send_secret_recovery_messages)
            SendSecretRecoveryMessages(dead_relay);
        ScheduleTaskToCheckWhichQuarterHoldersHaveRespondedToDeath(Death(dead_relay->number));
    }
}

void RelaySuccessionHandler::ScheduleTaskToCheckWhichQuarterHoldersHaveRespondedToDeath(uint160 death)
{
    scheduler.Schedule("death", death, GetTimeMicros() + RESPONSE_WAIT_TIME);
}

void RelaySuccessionHandler::SendSecretRecoveryMessages(Relay *dead_relay)
{
    if (dead_relay->current_successor_number == 0)
    {
        log_ << "can't send recovery message: " << dead_relay->number << " has no successor\n";
        return;
    }

    for(uint32_t position = 0; position < 4; position++)
    {
        auto quarter_holder = relay_state->GetRelayByNumber(dead_relay->key_quarter_holders[position]);
        if (data.keydata[quarter_holder->public_signing_key].HasProperty("privkey"))
            SendSecretRecoveryMessage(dead_relay, position);
    }
}

void RelaySuccessionHandler::SendSecretRecoveryMessage(Relay *dead_relay, uint32_t position)
{
    auto secret_recovery_message = GenerateSecretRecoveryMessage(dead_relay, position);
    data.StoreMessage(secret_recovery_message);
    relay_message_handler->Broadcast(secret_recovery_message);
    relay_message_handler->Handle(secret_recovery_message, NULL);
}

SecretRecoveryMessage RelaySuccessionHandler::GenerateSecretRecoveryMessage(Relay *dead_relay, uint32_t position)
{
    auto quarter_holder = relay_state->GetRelayByNumber(dead_relay->key_quarter_holders[position]);

    SecretRecoveryMessage secret_recovery_message;
    secret_recovery_message.position_of_quarter_holder = (uint8_t) position;
    secret_recovery_message.dead_relay_number = dead_relay->number;
    secret_recovery_message.quarter_holder_number = quarter_holder->number;
    secret_recovery_message.successor_number = dead_relay->current_successor_number;
    secret_recovery_message.PopulateSecrets(data);
    secret_recovery_message.Sign(data);

    return secret_recovery_message;
}

void RelaySuccessionHandler::HandleDeathAfterDuration(uint160 death)
{
    if (RelayWasNonResponsiveAndHasNowResponded(death))
        return;

    MarkKeyQuarterHoldersWhoHaventRespondedToDeathAsNotResponding(death);
}

bool RelaySuccessionHandler::RelayWasNonResponsiveAndHasNowResponded(uint160 death)
{
    return false;
}

void RelaySuccessionHandler::MarkKeyQuarterHoldersWhoHaventRespondedToDeathAsNotResponding(uint160 death)
{
    auto key_quarter_holders_who_havent_responded = GetKeyQuarterHoldersWhoHaventRespondedToDeath(death);
    for (auto quarter_holder_number : key_quarter_holders_who_havent_responded)
    {
        DurationWithoutResponseFromRelay duration;
        duration.message_hash = death;
        duration.relay_number = quarter_holder_number;
        data.StoreMessage(duration);
        relay_state->ProcessDurationWithoutResponseFromRelay(duration, data);
        relay_message_handler->AcceptForEncodingInChainIfLive(duration.GetHash160());
    }
    HandleNewlyDeadRelays();
}

vector<uint64_t> RelaySuccessionHandler::GetKeyQuarterHoldersWhoHaventRespondedToDeath(uint160 death)
{
    auto dead_relay = relay_state->GetRelayByNumber(GetDeadRelayNumber(death));

    vector<uint64_t> quarter_holders;
    for (auto quarter_holder_number : dead_relay->key_quarter_holders)
        quarter_holders.push_back(quarter_holder_number);

    for (auto &attempt : dead_relay->succession_attempts)
        for (auto &recovery_message_hash : attempt.second.recovery_message_hashes)
        {
            SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hash);
            EraseEntryFromVector(recovery_message.quarter_holder_number, quarter_holders);
        }

    return quarter_holders;
}

void RelaySuccessionHandler::HandleSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message)
{
    if (not ValidateSecretRecoveryMessage(secret_recovery_message))
    {
        relay_message_handler->RejectMessage(secret_recovery_message.GetHash160());
        return;
    }
    AcceptSecretRecoveryMessage(secret_recovery_message);
}

void RelaySuccessionHandler::AcceptSecretRecoveryMessage(SecretRecoveryMessage &secret_recovery_message)
{
    data.StoreMessage(secret_recovery_message);
    relay_state->ProcessSecretRecoveryMessage(secret_recovery_message);

    bool four_messages_received = AllFourSecretRecoveryMessagesHaveBeenReceived(secret_recovery_message);

    if (four_messages_received)
    {
        auto dead_relay = secret_recovery_message.GetDeadRelay(data);
        auto &attempt = dead_relay->succession_attempts[secret_recovery_message.successor_number];
        auto four_messages = attempt.GetFourSecretRecoveryMessages();
        data.StoreMessage(four_messages);
    }

    if (relay_message_handler->mode != LIVE)
        return;

    relay_message_handler->AcceptForEncodingInChainIfLive(secret_recovery_message.GetHash160());

    if (RelaysPrivateSigningKeyIsAvailable(secret_recovery_message.successor_number))
        ComplainIfThereAreBadEncryptedSecretsInSecretRecoveryMessage(secret_recovery_message);

    if (four_messages_received)
        HandleFourSecretRecoveryMessages(secret_recovery_message);
}

bool
RelaySuccessionHandler::AllFourSecretRecoveryMessagesHaveBeenReceived(SecretRecoveryMessage &secret_recovery_message)
{
    auto dead_relay = secret_recovery_message.GetDeadRelay(data);
    for (auto &attempt : dead_relay->succession_attempts)
        if (attempt.second.HasFourRecoveryMessages())
            return true;
    return false;
}

void RelaySuccessionHandler::HandleFourSecretRecoveryMessages(SecretRecoveryMessage &recovery_message)
{
    auto dead_relay = recovery_message.GetDeadRelay(data);
    auto &attempt = dead_relay->succession_attempts[recovery_message.successor_number];
    auto four_messages = attempt.GetFourSecretRecoveryMessages();
    scheduler.Schedule("secret_recovery", four_messages.GetHash160(), GetTimeMicros() + RESPONSE_WAIT_TIME);

    if (RelaysPrivateSigningKeyIsAvailable(recovery_message.successor_number))
        if (TryToRetrieveSecretsFromSecretRecoveryMessage(recovery_message) and send_succession_completed_messages)
        {
            if (relay_message_handler->mode == LIVE)
            {
                SendSuccessionCompletedMessage(recovery_message);
                HandleTasksInheritedFromDeadRelay(recovery_message);
            }
        }
}

void RelaySuccessionHandler::SendSuccessionCompletedMessage(SecretRecoveryMessage &recovery_message)
{
    auto succession_completed_message = GenerateSuccessionCompletedMessage(recovery_message);
    relay_message_handler->Broadcast(succession_completed_message);
    relay_message_handler->Handle(succession_completed_message, NULL);
}

SuccessionCompletedMessage
RelaySuccessionHandler::GenerateSuccessionCompletedMessage(SecretRecoveryMessage &recovery_message)
{
    auto successor = recovery_message.GetSuccessor(data);
    return successor->GenerateSuccessionCompletedMessage(recovery_message, data);
}

void RelaySuccessionHandler::HandleTasksInheritedFromDeadRelay(SecretRecoveryMessage &recovery_message)
{
    auto successor = recovery_message.GetSuccessor(data);
    inherited_task_handler.HandleInheritedTasks(successor);
}

void RelaySuccessionHandler::HandleFourSecretRecoveryMessagesAfterDuration(uint160 hash_of_four_recovery_messages)
{
    if (ThereHasBeenAResponseToTheSecretRecoveryMessages(hash_of_four_recovery_messages))
        return;
    DurationWithoutResponse duration;
    duration.message_hash = hash_of_four_recovery_messages;
    data.StoreMessage(duration);
    relay_state->ProcessDurationWithoutResponse(duration, data);
    relay_message_handler->AcceptForEncodingInChainIfLive(duration.GetHash160());
}

bool RelaySuccessionHandler::ThereHasBeenAResponseToTheSecretRecoveryMessages(uint160 hash_of_four_recovery_messages)
{
    FourSecretRecoveryMessages four_messages = data.GetMessage(hash_of_four_recovery_messages);
    SecretRecoveryMessage recovery_message = data.GetMessage(four_messages.secret_recovery_messages[0]);

    auto dead_relay = recovery_message.GetDeadRelay(data);

    if (dead_relay == NULL)
        return true;
    auto &attempt = dead_relay->succession_attempts[recovery_message.successor_number];
    if (attempt.succession_completed_message_hash != 0 or not attempt.HasFourRecoveryMessages())
        return true;;

    return false;
}

void RelaySuccessionHandler::ComplainIfThereAreBadEncryptedSecretsInSecretRecoveryMessage(SecretRecoveryMessage
                                                                                          secret_recovery_message)
{
    auto shared_secret_quarters = secret_recovery_message.RecoverSharedSecretQuartersForRelayKeyParts(data);

    for (uint32_t key_sharer_position = 0; key_sharer_position < shared_secret_quarters.size(); key_sharer_position++)
    {
        for (uint32_t key_part_position = 0;
             key_part_position < shared_secret_quarters[key_sharer_position].size();
             key_part_position++)
        {
            auto shared_secret_quarter = shared_secret_quarters[key_sharer_position][key_part_position];
            if (shared_secret_quarter.IsAtInfinity() and send_secret_recovery_complaints)
            {
                SendSecretRecoveryComplaint(secret_recovery_message, key_sharer_position, key_part_position);
                return;
            }
        }
    }
}

void RelaySuccessionHandler::SendSecretRecoveryComplaint(SecretRecoveryMessage recovery_message,
                                                         uint32_t key_sharer_position,
                                                         uint32_t key_part_position)
{
    auto complaint = GenerateSecretRecoveryComplaint(recovery_message, key_sharer_position, key_part_position);
    relay_message_handler->Handle(complaint, NULL);
    relay_message_handler->Broadcast(complaint);
}

SecretRecoveryComplaint RelaySuccessionHandler::GenerateSecretRecoveryComplaint(SecretRecoveryMessage recovery_message,
                                                                                uint32_t key_sharer_position,
                                                                                uint32_t key_part_position)
{
    SecretRecoveryComplaint complaint;
    complaint.Populate(recovery_message, key_sharer_position, key_part_position, data);
    complaint.Sign(data);
    return complaint;
}

bool RelaySuccessionHandler::ValidateSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message)
{
    return secret_recovery_message.IsValid(data) and secret_recovery_message.VerifySignature(data);
}

bool RelaySuccessionHandler::RelaysPrivateSigningKeyIsAvailable(uint64_t relay_number)
{
    auto relay = relay_state->GetRelayByNumber(relay_number);
    if (relay == NULL)
        return false;
    return data.keydata[relay->public_signing_key].HasProperty("privkey");
}

bool RelaySuccessionHandler::TryToRetrieveSecretsFromSecretRecoveryMessage(SecretRecoveryMessage recovery_message)
{
    auto dead_relay = recovery_message.GetDeadRelay(data);

    for (auto &attempt : dead_relay->succession_attempts)
        if (attempt.second.HasFourRecoveryMessages() and ArrayContainsEntry(attempt.second.recovery_message_hashes, recovery_message.GetHash160()))
            return TryToRecoverSecretsFromSecretRecoveryMessages(attempt.second.recovery_message_hashes);

    return false;
}

bool RelaySuccessionHandler::TryToRecoverSecretsFromSecretRecoveryMessages(
        std::array<uint160, 4> recovery_message_hashes)
{
    uint32_t failure_sharer_position, failure_key_part_position;
    bool succeeded = SecretRecoveryMessage::RecoverSecretsFromSecretRecoveryMessages(recovery_message_hashes,
                                                                                     failure_sharer_position,
                                                                                     failure_key_part_position, data);
    if (not succeeded)
        SendSecretRecoveryFailureMessage(recovery_message_hashes);
    return succeeded;
}

void RelaySuccessionHandler::SendSecretRecoveryFailureMessage(std::array<uint160, 4> recovery_message_hashes)
{
    auto failure_message = GenerateSecretRecoveryFailureMessage(recovery_message_hashes);
    relay_message_handler->Handle(failure_message, NULL);
    relay_message_handler->Broadcast(failure_message);
}

SecretRecoveryFailureMessage RelaySuccessionHandler::GenerateSecretRecoveryFailureMessage(
        std::array<uint160, 4> recovery_message_hashes)
{
    SecretRecoveryFailureMessage failure_message;
    failure_message.Populate(recovery_message_hashes, data);
    failure_message.Sign(data);
    return failure_message;
}

void RelaySuccessionHandler::HandleSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message)
{
    if (not ValidateSecretRecoveryFailureMessage(failure_message))
    {
        relay_message_handler->RejectMessage(failure_message.GetHash160());
        return;
    }
    AcceptSecretRecoveryFailureMessage(failure_message);
}

void RelaySuccessionHandler::AcceptSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message)
{
    StoreSecretRecoveryFailureMessage(failure_message);
    relay_message_handler->AcceptForEncodingInChainIfLive(failure_message.GetHash160());
    relay_state->ProcessSecretRecoveryFailureMessage(failure_message, data);
    if (relay_message_handler->mode == LIVE)
    {
        SendAuditMessagesInResponseToFailureMessage(failure_message);
        scheduler.Schedule("secret_recovery_failure", failure_message.GetHash160(),
                           GetTimeMicros() + RESPONSE_WAIT_TIME);
    }
}

bool RelaySuccessionHandler::ValidateSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message)
{
    return failure_message.IsValid(data) and failure_message.VerifySignature(data);
}

void RelaySuccessionHandler::SendAuditMessagesInResponseToFailureMessage(SecretRecoveryFailureMessage failure_message)
{
    for (auto recovery_message_hash : failure_message.recovery_message_hashes)
    {
        SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hash);
        auto quarter_holder = relay_state->GetRelayByNumber(recovery_message.quarter_holder_number);
        if (data.keydata[quarter_holder->public_signing_key].HasProperty("privkey") and send_audit_messages)
            SendAuditMessageFromQuarterHolderInResponseToFailureMessage(failure_message, recovery_message);
    }
}

void RelaySuccessionHandler::SendAuditMessageFromQuarterHolderInResponseToFailureMessage(
        SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message)
{
    RecoveryFailureAuditMessage audit_message;
    audit_message.Populate(failure_message, recovery_message, data);
    audit_message.Sign(data);
    relay_message_handler->Handle(audit_message, NULL);
    relay_message_handler->Broadcast(audit_message);
}

void RelaySuccessionHandler::StoreSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message)
{
    data.StoreMessage(failure_message);
}

void RelaySuccessionHandler::HandleSecretRecoveryComplaint(SecretRecoveryComplaint complaint)
{
    if (not ValidateSecretRecoveryComplaint(complaint))
    {
        relay_message_handler->RejectMessage(complaint.GetHash160());
        return;
    }
    AcceptSecretRecoveryComplaint(complaint);
}

void RelaySuccessionHandler::AcceptSecretRecoveryComplaint(SecretRecoveryComplaint &complaint)
{
    data.StoreMessage(complaint);
    relay_state->ProcessSecretRecoveryComplaint(complaint, data);
    relay_message_handler->AcceptForEncodingInChainIfLive(complaint.GetHash160());
    relay_message_handler->HandleNewlyDeadRelays();
}

bool RelaySuccessionHandler::ValidateSecretRecoveryComplaint(SecretRecoveryComplaint &complaint)
{
    return complaint.IsValid(data) and complaint.VerifySignature(data);
}

void RelaySuccessionHandler::HandleRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message)
{
    if (not ValidateRecoveryFailureAuditMessage(audit_message))
    {
        relay_message_handler->RejectMessage(audit_message.GetHash160());
        return;
    }
    AcceptRecoveryFailureAuditMessage(audit_message);
}

void RelaySuccessionHandler::AcceptRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message)
{
    StoreRecoveryFailureAuditMessage(audit_message);
    relay_state->ProcessRecoveryFailureAuditMessage(audit_message, data);
    relay_message_handler->AcceptForEncodingInChainIfLive(audit_message.GetHash160());
}

bool RelaySuccessionHandler::ValidateRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message)
{
    return audit_message.IsValid(data) and audit_message.VerifySignature(data);
}

void RelaySuccessionHandler::StoreRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message)
{
    data.StoreMessage(audit_message);
    uint160 audit_message_hash = audit_message.GetHash160();
    RecordResponseToMessage(audit_message_hash, audit_message.failure_message_hash, "audit_messages");
}

void RelaySuccessionHandler::HandleSecretRecoveryFailureMessageAfterDuration(uint160 failure_message_hash)
{
    auto non_responding_quarter_holders = RelaysNotRespondingToRecoveryFailureMessage(failure_message_hash);
    for (auto quarter_holder_number : non_responding_quarter_holders)
    {
        DurationWithoutResponseFromRelay duration;
        duration.relay_number = quarter_holder_number;
        duration.message_hash = failure_message_hash;
        relay_state->ProcessDurationWithoutResponseFromRelay(duration, data);
        relay_message_handler->AcceptForEncodingInChainIfLive(duration.GetHash160());
    }
    HandleNewlyDeadRelays();
}

vector<uint64_t> RelaySuccessionHandler::RelaysNotRespondingToRecoveryFailureMessage(uint160 failure_message_hash)
{
    SecretRecoveryFailureMessage failure_message = data.GetMessage(failure_message_hash);
    auto dead_relay = failure_message.GetDeadRelay(data);

    vector<uint64_t> remaining_quarter_holders;
    for (auto quarter_holder_number : dead_relay->key_quarter_holders)
        remaining_quarter_holders.push_back(quarter_holder_number);

    auto &attempt = dead_relay->succession_attempts[failure_message.successor_number];
    auto &audit = attempt.audits[failure_message_hash];

    for (auto audit_message_hash : audit.audit_message_hashes)
    {
        RecoveryFailureAuditMessage audit_message = data.GetMessage(audit_message_hash);
        EraseEntryFromVector(audit_message.quarter_holder_number, remaining_quarter_holders);
    }
    return remaining_quarter_holders;
}

void RelaySuccessionHandler::HandleGoodbyeMessage(GoodbyeMessage goodbye_message)
{
    if (not ValidateGoodbyeMessage(goodbye_message))
    {
        relay_message_handler->RejectMessage(goodbye_message.GetHash160());
        return;
    }
    AcceptGoodbyeMessage(goodbye_message);
}

void RelaySuccessionHandler::AcceptGoodbyeMessage(GoodbyeMessage &goodbye_message)
{
    relay_state->ProcessGoodbyeMessage(goodbye_message, data);
    relay_message_handler->AcceptForEncodingInChainIfLive(goodbye_message.GetHash160());
}

bool RelaySuccessionHandler::ValidateGoodbyeMessage(GoodbyeMessage &goodbye_message)
{
    return relay_state->ContainsRelayWithNumber(goodbye_message.dead_relay_number)
           and goodbye_message.VerifySignature(data);
}

void RelaySuccessionHandler::RecordResponseToMessage(uint160 response_hash, uint160 message_hash, string response_type)
{
    vector<uint160> response_hashes = data.msgdata[message_hash][response_type];

    if (not VectorContainsEntry(response_hashes, response_hash))
        response_hashes.push_back(response_hash);

    data.msgdata[message_hash][response_type] = response_hashes;
}

bool RelaySuccessionHandler::ValidateDurationWithoutResponseFromRelayAfterDeath(
        DurationWithoutResponseFromRelay &duration)
{
    auto non_responding_relay = relay_state->GetRelayByNumber(duration.relay_number);
    auto dead_relay = relay_state->GetRelayByNumber(GetDeadRelayNumber(duration.message_hash));

    for (auto &attempt : dead_relay->succession_attempts)
        for (auto recovery_message_hash : attempt.second.recovery_message_hashes)
        {
            SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hash);
            if (recovery_message.quarter_holder_number == duration.relay_number)
                return false;
        }
    return true;
}

bool
RelaySuccessionHandler::ValidateDurationWithoutResponseAfterSecretRecoveryMessage(DurationWithoutResponse &duration)
{
    SecretRecoveryMessage recovery_message = data.GetMessage(duration.message_hash);
    auto dead_relay = relay_state->GetRelayByNumber(recovery_message.dead_relay_number);
    if (dead_relay == NULL)
        return false;
    for (auto &attempt : dead_relay->succession_attempts)
        for (auto complaint_hash : attempt.second.recovery_complaint_hashes)
        {
            SecretRecoveryComplaint complaint = data.GetMessage(complaint_hash);
            if (complaint.secret_recovery_message_hash == duration.message_hash)
                return false;
        }
    return true;
}

bool RelaySuccessionHandler::ValidateDurationWithoutResponseFromRelayAfterSecretRecoveryFailureMessage(
        DurationWithoutResponseFromRelay &duration)
{
    SecretRecoveryFailureMessage failure_message = data.GetMessage(duration.message_hash);
    auto dead_relay = failure_message.GetDeadRelay(data);

    auto &attempt = dead_relay->succession_attempts[failure_message.successor_number];
    auto &audit = attempt.audits[failure_message.GetHash160()];

    for (auto audit_message_hash : audit.audit_message_hashes)
    {
        RecoveryFailureAuditMessage audit_message = data.GetMessage(audit_message_hash);
        if (audit_message.quarter_holder_number == duration.relay_number)
            return false;
    }
    return true;
}

void RelaySuccessionHandler::HandleSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message)
{
    if (not ValidateSuccessionCompletedMessage(succession_completed_message))
    {
        relay_message_handler->RejectMessage(succession_completed_message.GetHash160());
        return;
    }
    relay_state->ProcessSuccessionCompletedMessage(succession_completed_message, data);
}

bool
RelaySuccessionHandler::ValidateSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message)
{
    return succession_completed_message.IsValid(data) and succession_completed_message.VerifySignature(data);
}
