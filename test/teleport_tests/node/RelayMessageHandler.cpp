#include "RelayMessageHandler.h"
#include "Relay.h"
#include "RecoveryFailureAuditMessage.h"

#include "log.h"
#define LOG_CATEGORY "RelayMessageHandler.cpp"

using std::vector;
using std::string;

void RelayMessageHandler::SetCreditSystem(CreditSystem *credit_system_)
{
    credit_system = credit_system_;
}

void RelayMessageHandler::SetCalendar(Calendar *calendar_)
{
    calendar = calendar_;
}

void RelayMessageHandler::HandleRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    if (not ValidateRelayJoinMessage(relay_join_message))
    {
        RejectMessage(relay_join_message.GetHash160());
        return;
    }
    AcceptRelayJoinMessage(relay_join_message);
}

bool RelayMessageHandler::ValidateRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    if (not relay_join_message.ValidateSizes())
        return false;
    if (not MinedCreditMessageHashIsInMainChain(relay_join_message))
        return false;
    if (relay_state.MinedCreditMessageHashIsAlreadyBeingUsed(relay_join_message.mined_credit_message_hash))
        return false;
    if (not relay_join_message.VerifySignature(data))
        return false;
    if (MinedCreditMessageIsNewerOrMoreThanThreeBatchesOlderThanLatest(relay_join_message.mined_credit_message_hash))
        return false;;
    return true;
}

bool
RelayMessageHandler::MinedCreditMessageIsNewerOrMoreThanThreeBatchesOlderThanLatest(uint160 mined_credit_message_hash)
{
    MinedCreditMessage latest_msg = data.msgdata[relay_state.latest_mined_credit_message_hash]["msg"];
    MinedCreditMessage given_msg = data.msgdata[mined_credit_message_hash]["msg"];

    return given_msg.mined_credit.network_state.batch_number > latest_msg.mined_credit.network_state.batch_number or
            given_msg.mined_credit.network_state.batch_number + 3 < latest_msg.mined_credit.network_state.batch_number;
}

bool RelayMessageHandler::MinedCreditMessageHashIsInMainChain(RelayJoinMessage relay_join_message)
{
    return credit_system->IsInMainChain(relay_join_message.mined_credit_message_hash);
}

void RelayMessageHandler::AcceptRelayJoinMessage(RelayJoinMessage join_message)
{
    relay_state.ProcessRelayJoinMessage(join_message);
}

void RelayMessageHandler::HandleKeyDistributionMessage(KeyDistributionMessage key_distribution_message)
{
    if (not ValidateKeyDistributionMessage(key_distribution_message))
    {
        RejectMessage(key_distribution_message.GetHash160());
        return;
    }
    StoreKeyDistributionSecretsAndSendComplaints(key_distribution_message);
    relay_state.ProcessKeyDistributionMessage(key_distribution_message);
    scheduler.Schedule("key_distribution", key_distribution_message.GetHash160(), GetTimeMicros() + RESPONSE_WAIT_TIME);
}

void RelayMessageHandler::HandleKeyDistributionMessageAfterDuration(uint160 key_distribution_message_hash)
{
    vector<uint160> complaint_hashes = data.msgdata[key_distribution_message_hash]["complaints"];
    if (complaint_hashes.size() > 0)
        return;
    DurationWithoutResponse duration;
    duration.message_hash = key_distribution_message_hash;
    relay_state.ProcessDurationWithoutResponse(duration, data);
}

bool RelayMessageHandler::ValidateKeyDistributionMessage(KeyDistributionMessage key_distribution_message)
{
    if (not key_distribution_message.ValidateSizes())
        return false;
    if (not key_distribution_message.VerifySignature(data))
        return false;
    if (not key_distribution_message.VerifyRelayNumber(data))
        return false;;
    return true;
}

void RelayMessageHandler::StoreKeyDistributionSecretsAndSendComplaints(KeyDistributionMessage key_distribution_message)
{
    Relay *relay = relay_state.GetRelayByNumber(key_distribution_message.relay_number);

    RecoverSecretsAndSendKeyDistributionComplaints(
            relay, KEY_DISTRIBUTION_COMPLAINT_KEY_QUARTERS,
            relay_state.GetKeyQuarterHoldersAsListOf16RelayNumbers(relay->number),
            key_distribution_message.GetHash160(),
            key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders);

    RecoverSecretsAndSendKeyDistributionComplaints(
            relay, KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS,
            relay_state.GetKeySixteenthHoldersAsListOf16RelayNumbers(relay->number, 1),
            key_distribution_message.GetHash160(),
            key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders);

    RecoverSecretsAndSendKeyDistributionComplaints(
            relay, KEY_DISTRIBUTION_COMPLAINT_SECOND_KEY_SIXTEENTHS,
            relay_state.GetKeySixteenthHoldersAsListOf16RelayNumbers(relay->number, 2),
            key_distribution_message.GetHash160(),
            key_distribution_message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders);
}

void RelayMessageHandler::RecoverSecretsAndSendKeyDistributionComplaints(Relay *relay, uint64_t set_of_secrets,
                                                                         vector<uint64_t> recipients,
                                                                         uint160 key_distribution_message_hash,
                                                                         vector<CBigNum> encrypted_secrets)
{
    auto public_key_sixteenths = relay->PublicKeySixteenths();
    for (uint32_t position = 0; position < recipients.size(); position++)
    {
        Relay *recipient = data.relay_state->GetRelayByNumber(recipients[position]);

        if (not data.keydata[recipient->public_signing_key].HasProperty("privkey"))
            continue;

        bool ok = RecoverAndStoreSecret(recipient, encrypted_secrets[position], public_key_sixteenths[position])
                    and relay->public_key_set.VerifyRowOfGeneratedPoints(position, data.keydata);

        if (not ok)
        {
            SendKeyDistributionComplaint(key_distribution_message_hash, set_of_secrets, position);
        }
    }
}

bool RelayMessageHandler::RecoverAndStoreSecret(Relay *recipient, CBigNum &encrypted_secret,
                                                Point &point_corresponding_to_secret)
{
    CBigNum decrypted_secret = recipient->DecryptSecret(encrypted_secret, point_corresponding_to_secret, data);

    if (decrypted_secret == 0)
        return false;

    data.keydata[point_corresponding_to_secret]["privkey"] = decrypted_secret;
    return true;
}

void RelayMessageHandler::SendKeyDistributionComplaint(uint160 key_distribution_message_hash, uint64_t set_of_secrets,
                                                       uint32_t position_of_bad_secret)
{
    KeyDistributionComplaint complaint(key_distribution_message_hash, set_of_secrets, position_of_bad_secret, data);
    complaint.Sign(data);
    data.StoreMessage(complaint);
    Broadcast(complaint);
    RecordSentComplaint(complaint.GetHash160(), key_distribution_message_hash);
}

void RelayMessageHandler::RecordSentComplaint(uint160 complaint_hash, uint160 bad_message_hash)
{
    vector<uint160> complaints = data.msgdata[bad_message_hash]["complaints"];
    complaints.push_back(complaint_hash);
    data.msgdata[bad_message_hash]["complaints"] = complaints;
}

void RelayMessageHandler::HandleKeyDistributionComplaint(KeyDistributionComplaint complaint)
{
    if (not ValidateKeyDistributionComplaint(complaint))
    {
        RejectMessage(complaint.GetHash160());
        return;
    }
    relay_state.ProcessKeyDistributionComplaint(complaint, data);
    HandleNewlyDeadRelays();
}

void RelayMessageHandler::HandleNewlyDeadRelays()
{
    for (auto &relay : relay_state.relays)
        if (relay.hashes.obituary_hash != 0 and dead_relays.count(relay.number) == 0)
            HandleRelayDeath(&relay);
}

void RelayMessageHandler::HandleRelayDeath(Relay *dead_relay)
{
    dead_relays.insert(dead_relay->number);
    SendSecretRecoveryMessages(dead_relay);
    ScheduleTaskToCheckWhichQuarterHoldersHaveRespondedToObituary(dead_relay->hashes.obituary_hash);
}

bool RelayMessageHandler::ValidateKeyDistributionComplaint(KeyDistributionComplaint complaint)
{
    return complaint.IsValid(data) and complaint.VerifySignature(data);
}

void RelayMessageHandler::ScheduleTaskToCheckWhichQuarterHoldersHaveRespondedToObituary(uint160 obituary_hash)
{
    scheduler.Schedule("obituary", obituary_hash, GetTimeMicros() + RESPONSE_WAIT_TIME);
}

void RelayMessageHandler::SendSecretRecoveryMessages(Relay *dead_relay)
{
    for (auto quarter_holder_number : dead_relay->holders.key_quarter_holders)
    {
        auto quarter_holder = relay_state.GetRelayByNumber(quarter_holder_number);
        if (data.keydata[quarter_holder->public_signing_key].HasProperty("privkey"))
            SendSecretRecoveryMessage(dead_relay, quarter_holder);
    }
}

void RelayMessageHandler::SendSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder)
{
    auto secret_recovery_message = GenerateSecretRecoveryMessage(dead_relay, quarter_holder);
    StoreSecretRecoveryMessage(secret_recovery_message, dead_relay->hashes.obituary_hash);
    Broadcast(secret_recovery_message);
    Handle(secret_recovery_message, NULL);
}

SecretRecoveryMessage RelayMessageHandler::GenerateSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder)
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

void RelayMessageHandler::StoreSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message, uint160 obituary_hash)
{
    data.StoreMessage(secret_recovery_message);
    uint160 secret_recovery_message_hash = secret_recovery_message.GetHash160();
    RecordResponseToMessage(secret_recovery_message_hash, obituary_hash, "secret_recovery_messages");
}

void RelayMessageHandler::HandleObituaryAfterDuration(uint160 obituary_hash)
{
    ProcessKeyQuarterHoldersWhoHaventRespondedToObituary(obituary_hash);
}

void RelayMessageHandler::ProcessKeyQuarterHoldersWhoHaventRespondedToObituary(uint160 obituary_hash)
{
    auto key_quarter_holders_who_havent_responded = GetKeyQuarterHoldersWhoHaventRespondedToObituary(obituary_hash);
    for (auto quarter_holder_number : key_quarter_holders_who_havent_responded)
    {
        DurationWithoutResponseFromRelay duration;
        duration.message_hash = obituary_hash;
        duration.relay_number = quarter_holder_number;
        data.StoreMessage(duration);
        relay_state.ProcessDurationWithoutResponseFromRelay(duration, data);
    }
    HandleNewlyDeadRelays();
}

vector<uint64_t> RelayMessageHandler::GetKeyQuarterHoldersWhoHaventRespondedToObituary(uint160 obituary_hash)
{
    Obituary obituary = data.GetMessage(obituary_hash);
    auto dead_relay = relay_state.GetRelayByNumber(obituary.dead_relay_number);
    vector<uint64_t> quarter_holders = dead_relay->holders.key_quarter_holders;
    vector<uint160> secret_recovery_message_hashes = msgdata[obituary_hash]["secret_recovery_messages"];
    for (auto secret_recovery_message_hash : secret_recovery_message_hashes)
    {
        SecretRecoveryMessage secret_recovery_message = data.GetMessage(secret_recovery_message_hash);
        EraseEntryFromVector(secret_recovery_message.quarter_holder_number, quarter_holders);
    }
    return quarter_holders;
}

void RelayMessageHandler::HandleSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message)
{
    if (not ValidateSecretRecoveryMessage(secret_recovery_message))
    {
        RejectMessage(secret_recovery_message.GetHash160());
        return;
    }
    StoreSecretRecoveryMessage(secret_recovery_message, secret_recovery_message.obituary_hash);
    scheduler.Schedule("secret_recovery", secret_recovery_message.GetHash160(), GetTimeMicros() + RESPONSE_WAIT_TIME);

    if (RelaysPrivateSigningKeyIsAvailable(secret_recovery_message.successor_number))
    {
        ComplainIfThereAreBadEncryptedSecretsInSecretRecoveryMessage(secret_recovery_message);
        TryToRetrieveSecretsFromSecretRecoveryMessage(secret_recovery_message);
    }
}

void RelayMessageHandler::ComplainIfThereAreBadEncryptedSecretsInSecretRecoveryMessage(SecretRecoveryMessage
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

void RelayMessageHandler::SendSecretRecoveryComplaint(SecretRecoveryMessage recovery_message,
                                                      uint32_t key_sharer_position,
                                                      uint32_t key_part_position)
{
    auto complaint = GenerateSecretRecoveryComplaint(recovery_message, key_sharer_position, key_part_position);
    Handle(complaint, NULL);
    Broadcast(complaint);
}

SecretRecoveryComplaint RelayMessageHandler::GenerateSecretRecoveryComplaint(SecretRecoveryMessage recovery_message,
                                                                             uint32_t key_sharer_position,
                                                                             uint32_t key_part_position)
{
    SecretRecoveryComplaint complaint;
    complaint.Populate(recovery_message, key_sharer_position, key_part_position, data);
    complaint.Sign(data);
    return complaint;
}

bool RelayMessageHandler::ValidateSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message)
{
    return secret_recovery_message.IsValid(data) and secret_recovery_message.VerifySignature(data);
}

bool RelayMessageHandler::RelaysPrivateSigningKeyIsAvailable(uint64_t relay_number)
{
    auto relay = relay_state.GetRelayByNumber(relay_number);
    if (relay == NULL)
        return false;
    return data.keydata[relay->public_signing_key].HasProperty("privkey");
}

void RelayMessageHandler::TryToRetrieveSecretsFromSecretRecoveryMessage(SecretRecoveryMessage recovery_message)
{
    vector<uint160> recovery_message_hashes = msgdata[recovery_message.obituary_hash]["secret_recovery_messages"];

    if (recovery_message_hashes.size() != 4)
        return;

    TryToRecoverSecretsFromSecretRecoveryMessages(recovery_message_hashes);
}

bool RelayMessageHandler::TryToRecoverSecretsFromSecretRecoveryMessages(vector<uint160> recovery_message_hashes)
{
    uint32_t failure_sharer_position, failure_key_part_position;
    bool succeeded = SecretRecoveryMessage::RecoverSecretsFromSecretRecoveryMessages(recovery_message_hashes,
                                                                                     failure_sharer_position,
                                                                                     failure_key_part_position, data);

    if (not succeeded)
        SendSecretRecoveryFailureMessage(recovery_message_hashes);

    return succeeded;
}

void RelayMessageHandler::SendSecretRecoveryFailureMessage(vector<uint160> recovery_message_hashes)
{
    SecretRecoveryFailureMessage failure_message;
    failure_message.Populate(recovery_message_hashes, data);
    failure_message.Sign(data);
    Handle(failure_message, NULL);
    Broadcast(failure_message);
}

void RelayMessageHandler::HandleSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message)
{
    if (not ValidateSecretRecoveryFailureMessage(failure_message))
    {
        RejectMessage(failure_message.GetHash160());
        return;
    }
    StoreSecretRecoveryFailureMessage(failure_message);
    SendAuditMessagesInResponseToFailureMessage(failure_message);
    scheduler.Schedule("secret_recovery_failure", failure_message.GetHash160(), GetTimeMicros() + RESPONSE_WAIT_TIME);
}

bool RelayMessageHandler::ValidateSecretRecoveryFailureMessage(SecretRecoveryFailureMessage &failure_message)
{
    return failure_message.IsValid(data) and failure_message.VerifySignature(data);
}

void RelayMessageHandler::SendAuditMessagesInResponseToFailureMessage(SecretRecoveryFailureMessage failure_message)
{
    for (auto recovery_message_hash : failure_message.recovery_message_hashes)
    {
        SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hash);
        auto quarter_holder = relay_state.GetRelayByNumber(recovery_message.quarter_holder_number);
        if (data.keydata[quarter_holder->public_signing_key].HasProperty("privkey") and send_audit_messages)
            SendAuditMessageFromQuarterHolderInResponseToFailureMessage(failure_message, recovery_message);
    }
}

void RelayMessageHandler::SendAuditMessageFromQuarterHolderInResponseToFailureMessage(
        SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message)
{
    RecoveryFailureAuditMessage audit_message;
    audit_message.Populate(failure_message, recovery_message, data);
    audit_message.Sign(data);
    Handle(audit_message, NULL);
    Broadcast(audit_message);
}

void RelayMessageHandler::StoreSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message)
{
    data.StoreMessage(failure_message);
    uint160 failure_message_hash = failure_message.GetHash160();
    RecordResponseToMessage(failure_message_hash, failure_message.obituary_hash, "failure_messages");
}

void RelayMessageHandler::HandleSecretRecoveryComplaint(SecretRecoveryComplaint complaint)
{
    if (not ValidateSecretRecoveryComplaint(complaint))
    {
        RejectMessage(complaint.GetHash160());
        return;
    }
    StoreSecretRecoveryComplaint(complaint);
    relay_state.ProcessSecretRecoveryComplaint(complaint, data);
}

bool RelayMessageHandler::ValidateSecretRecoveryComplaint(SecretRecoveryComplaint &complaint)
{
    return complaint.IsValid(data) and complaint.VerifySignature(data);
}

void RelayMessageHandler::StoreSecretRecoveryComplaint(SecretRecoveryComplaint complaint)
{
    data.StoreMessage(complaint);
    uint160 complaint_hash = complaint.GetHash160();
    RecordResponseToMessage(complaint_hash, complaint.secret_recovery_message_hash, "complaints");
}

void RelayMessageHandler::HandleRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message)
{
    if (not ValidateRecoveryFailureAuditMessage(audit_message))
    {
        RejectMessage(audit_message.GetHash160());
        return;
    }
    StoreRecoveryFailureAuditMessage(audit_message);
    relay_state.ProcessRecoveryFailureAuditMessage(audit_message, data);
}

bool RelayMessageHandler::ValidateRecoveryFailureAuditMessage(RecoveryFailureAuditMessage &audit_message)
{
    return audit_message.IsValid(data) and audit_message.VerifySignature(data);
}

void RelayMessageHandler::StoreRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message)
{
    data.StoreMessage(audit_message);
    uint160 audit_message_hash = audit_message.GetHash160();
    RecordResponseToMessage(audit_message_hash, audit_message.failure_message_hash, "audit_messages");
}

void RelayMessageHandler::HandleSecretRecoveryFailureMessageAfterDuration(uint160 failure_message_hash)
{
    auto non_responding_quarter_holders = RelaysNotRespondingToRecoveryFailureMessage(failure_message_hash);
    for (auto quarter_holder_number : non_responding_quarter_holders)
    {
        DurationWithoutResponseFromRelay duration;
        duration.relay_number = quarter_holder_number;
        duration.message_hash = failure_message_hash;
        relay_state.ProcessDurationWithoutResponseFromRelay(duration, data);
    }
    HandleNewlyDeadRelays();
}

vector<uint64_t> RelayMessageHandler::RelaysNotRespondingToRecoveryFailureMessage(uint160 failure_message_hash)
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

void RelayMessageHandler::HandleGoodbyeMessage(GoodbyeMessage goodbye_message)
{
    if (not ValidateGoodbyeMessage(goodbye_message))
    {
        RejectMessage(goodbye_message.GetHash160());
        return;
    }
    auto successor = goodbye_message.GetSuccessor(data);
    if (data.keydata[successor->public_signing_key].HasProperty("privkey"))
        TryToExtractSecretsFromGoodbyeMessage(goodbye_message);
    relay_state.ProcessGoodbyeMessage(goodbye_message);
    scheduler.Schedule("goodbye", goodbye_message.GetHash160(), GetTimeMicros() + RESPONSE_WAIT_TIME);
}

bool RelayMessageHandler::ValidateGoodbyeMessage(GoodbyeMessage &goodbye_message)
{
    return goodbye_message.IsValid(data) and goodbye_message.VerifySignature(data);
}

void RelayMessageHandler::TryToExtractSecretsFromGoodbyeMessage(GoodbyeMessage goodbye_message)
{
    if (not (ExtractSecretsFromGoodbyeMessage(goodbye_message)))
        SendGoodbyeComplaint(goodbye_message);
}

bool RelayMessageHandler::ExtractSecretsFromGoodbyeMessage(GoodbyeMessage goodbye_message)
{
    return goodbye_message.ExtractSecrets(data);
}

void RelayMessageHandler::SendGoodbyeComplaint(GoodbyeMessage goodbye_message)
{
    GoodbyeComplaint complaint;
    complaint.Populate(goodbye_message, data);
    complaint.Sign(data);
    Handle(complaint, NULL);
    Broadcast(complaint);
}

void RelayMessageHandler::HandleGoodbyeMessageAfterDuration(uint160 goodbye_message_hash)
{
    DurationWithoutResponse duration;
    duration.message_hash = goodbye_message_hash;
    relay_state.ProcessDurationWithoutResponse(duration, data);
}

void RelayMessageHandler::HandleGoodbyeComplaint(GoodbyeComplaint complaint)
{
    if (not ValidateGoodbyeComplaint(complaint))
    {
        RejectMessage(complaint.GetHash160());
        return;
    }
    StoreGoodbyeComplaint(complaint);
    relay_state.ProcessGoodbyeComplaint(complaint, data);
    HandleNewlyDeadRelays();
}

bool RelayMessageHandler::ValidateGoodbyeComplaint(GoodbyeComplaint &complaint)
{
    return complaint.IsValid(data) and complaint.VerifySignature(data);
}

void RelayMessageHandler::StoreGoodbyeComplaint(GoodbyeComplaint &complaint)
{
    data.StoreMessage(complaint);
    uint160 complaint_hash = complaint.GetHash160();
    RecordResponseToMessage(complaint_hash, complaint.goodbye_message_hash, "complaints");
}

void RelayMessageHandler::RecordResponseToMessage(uint160 response_hash, uint160 message_hash, string response_type)
{
    vector<uint160> response_hashes = msgdata[message_hash][response_type];

    if (not VectorContainsEntry(response_hashes, response_hash))
        response_hashes.push_back(response_hash);

    msgdata[message_hash][response_type] = response_hashes;
}
