#include "RelaySuccessionHandler.h"
#include "RelayMessageHandler.h"
#include "test/teleport_tests/node/credit_handler/MinedCreditMessageBuilder.h"


#include "log.h"
#define LOG_CATEGORY "RelaySuccessionHandler.cpp"

using std::vector;
using std::string;
using std::set;

RelaySuccessionHandler::RelaySuccessionHandler(Data data_, CreditSystem *credit_system, Calendar *calendar,
                                               RelayMessageHandler *handler):
        data(data_), relay_state(&handler->relay_state), credit_system(credit_system), calendar(calendar),
        relay_message_handler(handler)
{
    data.relay_state = relay_state;
}

void RelaySuccessionHandler::HandleNewlyDeadRelays()
{
    for (auto &relay : relay_state->relays)
        if (relay.hashes.obituary_hash != 0 and dead_relays.count(relay.number) == 0)
            HandleRelayDeath(&relay);
}

void RelaySuccessionHandler::HandleRelayDeath(Relay *dead_relay)
{
    dead_relays.insert(dead_relay->number);

    if (relay_message_handler->mode == LIVE)
    {
        if (send_secret_recovery_messages)
            SendSecretRecoveryMessages(dead_relay);
        ScheduleTaskToCheckWhichQuarterHoldersHaveRespondedToObituary(dead_relay->hashes.obituary_hash);
    }
}

void RelaySuccessionHandler::ScheduleTaskToCheckWhichQuarterHoldersHaveRespondedToObituary(uint160 obituary_hash)
{
    scheduler.Schedule("obituary", obituary_hash, GetTimeMicros() + RESPONSE_WAIT_TIME);
}

void RelaySuccessionHandler::SendSecretRecoveryMessages(Relay *dead_relay)
{
    for (auto quarter_holder_number : dead_relay->holders.key_quarter_holders)
    {
        auto quarter_holder = relay_state->GetRelayByNumber(quarter_holder_number);
        if (data.keydata[quarter_holder->public_signing_key].HasProperty("privkey"))
            SendSecretRecoveryMessage(dead_relay, quarter_holder);
    }
}

void RelaySuccessionHandler::SendSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder)
{
    auto secret_recovery_message = GenerateSecretRecoveryMessage(dead_relay, quarter_holder);
    StoreSecretRecoveryMessage(secret_recovery_message, dead_relay->hashes.obituary_hash);
    relay_message_handler->Broadcast(secret_recovery_message);
    relay_message_handler->Handle(secret_recovery_message, NULL);
}

SecretRecoveryMessage RelaySuccessionHandler::GenerateSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder)
{
    SecretRecoveryMessage secret_recovery_message;

    secret_recovery_message.dead_relay_number = dead_relay->number;
    secret_recovery_message.quarter_holder_number = quarter_holder->number;
    secret_recovery_message.obituary_hash = dead_relay->hashes.obituary_hash;
    secret_recovery_message.successor_number = dead_relay->SuccessorNumber(data);
    secret_recovery_message.PopulateSecrets(data);
    secret_recovery_message.Sign(data);

    return secret_recovery_message;
}

void RelaySuccessionHandler::StoreSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message, uint160 obituary_hash)
{
    data.StoreMessage(secret_recovery_message);
    uint160 secret_recovery_message_hash = secret_recovery_message.GetHash160();
    RecordResponseToMessage(secret_recovery_message_hash, obituary_hash, "secret_recovery_messages");
}

void RelaySuccessionHandler::HandleObituaryAfterDuration(uint160 obituary_hash)
{
    ProcessKeyQuarterHoldersWhoHaventRespondedToObituary(obituary_hash);
}

void RelaySuccessionHandler::ProcessKeyQuarterHoldersWhoHaventRespondedToObituary(uint160 obituary_hash)
{
    auto key_quarter_holders_who_havent_responded = GetKeyQuarterHoldersWhoHaventRespondedToObituary(obituary_hash);
    for (auto quarter_holder_number : key_quarter_holders_who_havent_responded)
    {
        DurationWithoutResponseFromRelay duration;
        duration.message_hash = obituary_hash;
        duration.relay_number = quarter_holder_number;
        data.StoreMessage(duration);
        relay_state->ProcessDurationWithoutResponseFromRelay(duration, data);
        relay_message_handler->EncodeInChainIfLive(duration.GetHash160());
    }
    HandleNewlyDeadRelays();
}

vector<uint64_t> RelaySuccessionHandler::GetKeyQuarterHoldersWhoHaventRespondedToObituary(uint160 obituary_hash)
{
    Obituary obituary = data.GetMessage(obituary_hash);
    auto dead_relay = relay_state->GetRelayByNumber(obituary.dead_relay_number);
    vector<uint64_t> quarter_holders = dead_relay->holders.key_quarter_holders;
    vector<uint160> secret_recovery_message_hashes = data.msgdata[obituary_hash]["secret_recovery_messages"];
    for (auto secret_recovery_message_hash : secret_recovery_message_hashes)
    {
        SecretRecoveryMessage secret_recovery_message = data.GetMessage(secret_recovery_message_hash);
        EraseEntryFromVector(secret_recovery_message.quarter_holder_number, quarter_holders);
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
    StoreSecretRecoveryMessage(secret_recovery_message, secret_recovery_message.obituary_hash);
    relay_state->ProcessSecretRecoveryMessage(secret_recovery_message);

    if (relay_message_handler->mode != LIVE)
        return;
    relay_message_handler->EncodeInChainIfLive(secret_recovery_message.GetHash160());
    scheduler.Schedule("secret_recovery", secret_recovery_message.GetHash160(), GetTimeMicros() + RESPONSE_WAIT_TIME);

    if (RelaysPrivateSigningKeyIsAvailable(secret_recovery_message.successor_number))
    {
        ComplainIfThereAreBadEncryptedSecretsInSecretRecoveryMessage(secret_recovery_message);
        TryToRetrieveSecretsFromSecretRecoveryMessage(secret_recovery_message);
    }
}

void RelaySuccessionHandler::HandleSecretRecoveryMessageAfterDuration(uint160 recovery_message_hash)
{
    DurationWithoutResponse duration;
    duration.message_hash = recovery_message_hash;
    relay_state->ProcessDurationWithoutResponse(duration, data);
    relay_message_handler->EncodeInChainIfLive(duration.GetHash160());
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
            if (shared_secret_quarters[key_sharer_position][key_part_position].IsAtInfinity())
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

void RelaySuccessionHandler::TryToRetrieveSecretsFromSecretRecoveryMessage(SecretRecoveryMessage recovery_message)
{
    vector<uint160> recovery_message_hashes = data.msgdata[recovery_message.obituary_hash]["secret_recovery_messages"];

    if (recovery_message_hashes.size() != 4)
        return;

    TryToRecoverSecretsFromSecretRecoveryMessages(recovery_message_hashes);
}

bool RelaySuccessionHandler::TryToRecoverSecretsFromSecretRecoveryMessages(vector<uint160> recovery_message_hashes)
{
    uint32_t failure_sharer_position, failure_key_part_position;
    bool succeeded = SecretRecoveryMessage::RecoverSecretsFromSecretRecoveryMessages(recovery_message_hashes,
                                                                                     failure_sharer_position,
                                                                                     failure_key_part_position, data);

    if (not succeeded)
        SendSecretRecoveryFailureMessage(recovery_message_hashes);

    return succeeded;
}

void RelaySuccessionHandler::SendSecretRecoveryFailureMessage(vector<uint160> recovery_message_hashes)
{
    auto failure_message = GenerateSecretRecoveryFailureMessage(recovery_message_hashes);
    relay_message_handler->Handle(failure_message, NULL);
    relay_message_handler->Broadcast(failure_message);
}

SecretRecoveryFailureMessage RelaySuccessionHandler::GenerateSecretRecoveryFailureMessage(vector<uint160> recovery_message_hashes)
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
    relay_message_handler->EncodeInChainIfLive(failure_message.GetHash160());
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
    uint160 failure_message_hash = failure_message.GetHash160();
    RecordResponseToMessage(failure_message_hash, failure_message.obituary_hash, "failure_messages");
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
    StoreSecretRecoveryComplaint(complaint);
    relay_state->ProcessSecretRecoveryComplaint(complaint, data);
    relay_message_handler->EncodeInChainIfLive(complaint.GetHash160());
    relay_message_handler->HandleNewlyDeadRelays();
}

bool RelaySuccessionHandler::ValidateSecretRecoveryComplaint(SecretRecoveryComplaint &complaint)
{
    return complaint.IsValid(data) and complaint.VerifySignature(data);
}

void RelaySuccessionHandler::StoreSecretRecoveryComplaint(SecretRecoveryComplaint complaint)
{
    data.StoreMessage(complaint);
    uint160 complaint_hash = complaint.GetHash160();
    RecordResponseToMessage(complaint_hash, complaint.secret_recovery_message_hash, "complaints");
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
    relay_message_handler->EncodeInChainIfLive(audit_message.GetHash160());
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
        relay_message_handler->EncodeInChainIfLive(duration.GetHash160());
    }
    HandleNewlyDeadRelays();
}

vector<uint64_t> RelaySuccessionHandler::RelaysNotRespondingToRecoveryFailureMessage(uint160 failure_message_hash)
{
    SecretRecoveryFailureMessage failure_message = data.GetMessage(failure_message_hash);
    auto dead_relay = failure_message.GetDeadRelay(data);
    vector<uint64_t> remaining_quarter_holders = dead_relay->holders.key_quarter_holders;

    vector<uint160> audit_message_hashes = data.msgdata[failure_message_hash]["audit_messages"];
    for (auto audit_message_hash : audit_message_hashes)
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
    auto successor = goodbye_message.GetSuccessor(data);
    if (data.keydata[successor->public_signing_key].HasProperty("privkey"))
        TryToExtractSecretsFromGoodbyeMessage(goodbye_message);

    relay_state->ProcessGoodbyeMessage(goodbye_message);
    relay_message_handler->EncodeInChainIfLive(goodbye_message.GetHash160());

    if (relay_message_handler->mode == LIVE)
    {
        scheduler.Schedule("goodbye", goodbye_message.GetHash160(), GetTimeMicros() + RESPONSE_WAIT_TIME);
    }
}

bool RelaySuccessionHandler::ValidateGoodbyeMessage(GoodbyeMessage &goodbye_message)
{
    return goodbye_message.IsValid(data) and goodbye_message.VerifySignature(data);
}

void RelaySuccessionHandler::TryToExtractSecretsFromGoodbyeMessage(GoodbyeMessage goodbye_message)
{
    if (not ExtractSecretsFromGoodbyeMessage(goodbye_message) and relay_message_handler->mode == LIVE
                                                              and send_goodbye_complaints)
        SendGoodbyeComplaint(goodbye_message);
}

bool RelaySuccessionHandler::ExtractSecretsFromGoodbyeMessage(GoodbyeMessage goodbye_message)
{
    return goodbye_message.ExtractSecrets(data);
}

void RelaySuccessionHandler::SendGoodbyeComplaint(GoodbyeMessage goodbye_message)
{
    GoodbyeComplaint complaint;
    complaint.Populate(goodbye_message, data);
    complaint.Sign(data);
    relay_message_handler->Handle(complaint, NULL);
    relay_message_handler->Broadcast(complaint);
}

void RelaySuccessionHandler::HandleGoodbyeMessageAfterDuration(uint160 goodbye_message_hash)
{
    vector<uint160> complaint_hashes = data.msgdata[goodbye_message_hash]["complaints"];
    if (complaint_hashes.size() > 0)
        return;
    DurationWithoutResponse duration;
    duration.message_hash = goodbye_message_hash;
    relay_state->ProcessDurationWithoutResponse(duration, data);
    relay_message_handler->EncodeInChainIfLive(duration.GetHash160());
}

void RelaySuccessionHandler::HandleGoodbyeComplaint(GoodbyeComplaint complaint)
{
    if (not ValidateGoodbyeComplaint(complaint))
    {
        relay_message_handler->RejectMessage(complaint.GetHash160());
        return;
    }
    AcceptGoodbyeComplaint(complaint);
}

void RelaySuccessionHandler::AcceptGoodbyeComplaint(GoodbyeComplaint &complaint)
{
    StoreGoodbyeComplaint(complaint);
    relay_state->ProcessGoodbyeComplaint(complaint, data);
    relay_message_handler->EncodeInChainIfLive(complaint.GetHash160());
    HandleNewlyDeadRelays();
}

bool RelaySuccessionHandler::ValidateGoodbyeComplaint(GoodbyeComplaint &complaint)
{
    return complaint.IsValid(data) and complaint.VerifySignature(data);
}

void RelaySuccessionHandler::StoreGoodbyeComplaint(GoodbyeComplaint &complaint)
{
    data.StoreMessage(complaint);
    uint160 complaint_hash = complaint.GetHash160();
    RecordResponseToMessage(complaint_hash, complaint.goodbye_message_hash, "complaints");
}

void RelaySuccessionHandler::RecordResponseToMessage(uint160 response_hash, uint160 message_hash, string response_type)
{
    vector<uint160> response_hashes = data.msgdata[message_hash][response_type];

    if (not VectorContainsEntry(response_hashes, response_hash))
        response_hashes.push_back(response_hash);

    data.msgdata[message_hash][response_type] = response_hashes;
}

bool RelaySuccessionHandler::ValidateDurationWithoutResponseFromRelayAfterObituary(
        DurationWithoutResponseFromRelay &duration)
{
    Obituary obituary = data.GetMessage(duration.message_hash);
    auto non_responding_relay = relay_state->GetRelayByNumber(duration.relay_number);
    auto dead_relay = relay_state->GetRelayByNumber(obituary.dead_relay_number);
    for (auto recovery_message_hash : dead_relay->hashes.secret_recovery_message_hashes)
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
    for (auto complaint_hash : dead_relay->hashes.secret_recovery_complaint_hashes)
    {
        SecretRecoveryComplaint complaint = data.GetMessage(complaint_hash);
        if (complaint.secret_recovery_message_hash == duration.message_hash)
            return false;
    }
    return true;
}

bool RelaySuccessionHandler::ValidateDurationWithoutResponseAfterGoodbyeMessage(DurationWithoutResponse &duration)
{
    GoodbyeMessage goodbye = data.GetMessage(duration.message_hash);
    auto exiting_relay = relay_state->GetRelayByNumber(goodbye.dead_relay_number);
    if (exiting_relay == NULL)
        return false;
    return exiting_relay->hashes.goodbye_complaint_hashes.size() == 0;
}

bool RelaySuccessionHandler::ValidateDurationWithoutResponseFromRelayAfterSecretRecoveryFailureMessage(
        DurationWithoutResponseFromRelay &duration)
{
    SecretRecoveryFailureMessage failure_message = data.GetMessage(duration.message_hash);
    auto dead_relay = failure_message.GetDeadRelay(data);

    for (auto audit_message_hash : dead_relay->hashes.recovery_failure_audit_message_hashes)
    {
        RecoveryFailureAuditMessage audit_message = data.GetMessage(audit_message_hash);
        if (audit_message.quarter_holder_number == duration.relay_number)
            return false;
    }
    return true;
}
