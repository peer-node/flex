#include "RelayMessageHandler.h"
#include "Relay.h"

#include "log.h"
#define LOG_CATEGORY "RelayMessageHandler.cpp"

using std::vector;

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
{}

bool RelayMessageHandler::ValidateKeyDistributionMessage(KeyDistributionMessage key_distribution_message)
{
    if (key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders.size() != 16)
        return false;
    if (key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders.size() != 16)
        return false;
    if (key_distribution_message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders.size() != 16)
        return false;
    if (not key_distribution_message.VerifySignature(data))
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
    HandleRelayDeath(complaint.GetSecretSender(data));
}

void RelayMessageHandler::HandleRelayDeath(Relay *dead_relay)
{
    SendSecretRecoveryMessages(dead_relay);
    ScheduleTaskToCheckWhichQuarterHoldersHaveResponded(dead_relay->obituary_hash);
}

bool RelayMessageHandler::ValidateKeyDistributionComplaint(KeyDistributionComplaint complaint)
{
    return complaint.IsValid(data) and complaint.VerifySignature(data);
}

void RelayMessageHandler::ScheduleTaskToCheckWhichQuarterHoldersHaveResponded(uint160 obituary_hash)
{
    scheduler.Schedule("obituary", obituary_hash, GetTimeMicros() + RESPONSE_WAIT_TIME);
}

void RelayMessageHandler::SendSecretRecoveryMessages(Relay *dead_relay)
{
    for (auto quarter_holder_number : dead_relay->key_quarter_holders)
    {
        auto quarter_holder = relay_state.GetRelayByNumber(quarter_holder_number);
        if (data.keydata[quarter_holder->public_signing_key].HasProperty("privkey"))
            SendSecretRecoveryMessage(dead_relay, quarter_holder);
    }
}

void RelayMessageHandler::SendSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder)
{
    auto secret_recovery_message = GenerateSecretRecoveryMessage(dead_relay, quarter_holder);
    StoreSecretRecoveryMessage(secret_recovery_message, dead_relay->obituary_hash);
    Broadcast(secret_recovery_message);
    Handle(secret_recovery_message, NULL);
}

SecretRecoveryMessage RelayMessageHandler::GenerateSecretRecoveryMessage(Relay *dead_relay, Relay *quarter_holder)
{
    SecretRecoveryMessage secret_recovery_message;

    secret_recovery_message.dead_relay_number = dead_relay->number;
    secret_recovery_message.quarter_holder_number = quarter_holder->number;
    secret_recovery_message.obituary_hash = dead_relay->obituary_hash;
    secret_recovery_message.successor_number = dead_relay->SuccessorNumber(data);
    secret_recovery_message.PopulateSecrets(data);
    secret_recovery_message.Sign(data);

    return secret_recovery_message;
}

void RelayMessageHandler::StoreSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message, uint160 obituary_hash)
{
    data.StoreMessage(secret_recovery_message);
    vector<uint160> secret_recovery_message_hashes = msgdata[obituary_hash]["secret_recovery_messages"];
    uint160 secret_recovery_message_hash = secret_recovery_message.GetHash160();
    if (not VectorContainsEntry(secret_recovery_message_hashes, secret_recovery_message_hash))
        secret_recovery_message_hashes.push_back(secret_recovery_message_hash);
    msgdata[obituary_hash]["secret_recovery_messages"] = secret_recovery_message_hashes;
}

void RelayMessageHandler::HandleObituaryAfterDuration(uint160 obituary_hash)
{
    ProcessKeyQuarterHoldersWhoHaventRespondedToObituary(obituary_hash);
}

void RelayMessageHandler::ProcessKeyQuarterHoldersWhoHaventRespondedToObituary(uint160 obituary_hash)
{
    auto key_quarter_holders_who_havent_responded = GetKeyQuarterHoldersWhoHaventResponded(obituary_hash);
    for (auto quarter_holder_number : key_quarter_holders_who_havent_responded)
    {
        DurationWithoutResponseFromRelay duration;
        duration.message_hash = obituary_hash;
        duration.relay_number = quarter_holder_number;
        data.StoreMessage(duration);
        relay_state.ProcessDurationWithoutResponseFromRelay(duration, data);
    }
}

vector<uint64_t> RelayMessageHandler::GetKeyQuarterHoldersWhoHaventResponded(uint160 obituary_hash)
{
    Obituary obituary = data.GetMessage(obituary_hash);
    vector<uint64_t> quarter_holders = obituary.relay.key_quarter_holders;
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
        TryToRetrieveSecretsFromSecretRecoveryMessage(secret_recovery_message);
}

bool RelayMessageHandler::ValidateSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message)
{
    return true;
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

    if (not RecoverSecretsFromSecretRecoveryMessages(recovery_message_hashes))
        SendComplaintAboutFailedSecretRecovery(recovery_message_hashes);
}

void AddArrayOfPointsToSum(vector<vector<Point> > array_of_points, vector<vector<Point> > &sum)
{
    if (sum.size() == 0)
        sum.resize(array_of_points.size());
    
    for (int row = 0; row < array_of_points.size(); row++)
    {
        for (int column = 0; column < array_of_points[row].size(); column++)
        {
            if (sum[row].size() <= column)
                sum[row].push_back(Point(CBigNum(0)));
            sum[row][column] += array_of_points[row][column];
        }
    }
}

bool RelayMessageHandler::RecoverSecretsFromSecretRecoveryMessages(vector<uint160> recovery_message_hashes)
{
    vector<vector<Point> > array_of_shared_secrets;
    
    for (auto recovery_message_hash : recovery_message_hashes)
    {
        SecretRecoveryMessage secret_recovery_message = data.GetMessage(recovery_message_hash);
        auto shared_secret_quarters = secret_recovery_message.RecoverSharedSecretQuartersForRelayKeyParts(data);
        if (SomePointsAreAtInfinity(shared_secret_quarters))
            return false;
        AddArrayOfPointsToSum(shared_secret_quarters, array_of_shared_secrets);
    }
    return RecoverSecretsFromArrayOfSharedSecrets(array_of_shared_secrets, recovery_message_hashes);
}

bool RelayMessageHandler::SomePointsAreAtInfinity(vector<vector<Point> > array_of_points)
{
    for (auto &row : array_of_points)
        for (auto &point : row)
            if (point.IsAtInfinity())
                return true;
    return false;
}

bool RelayMessageHandler::RecoverSecretsFromArrayOfSharedSecrets(vector<vector<Point> > array_of_shared_secrets,
                                                                 vector<uint160> recovery_message_hashes)
{
    SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hashes[0]);

    for (int i = 0; i < recovery_message.key_quarter_sharers.size(); i++)
        if (not RecoverFourKeySixteenths(recovery_message.key_quarter_sharers[i], recovery_message.dead_relay_number,
                                         recovery_message.key_quarter_positions[i], array_of_shared_secrets[i]))
            return false;
    return true;
}

bool RelayMessageHandler::RecoverFourKeySixteenths(uint64_t key_sharer_number, uint64_t dead_relay_number,
                                                   uint8_t key_quarter_position, vector<Point> shared_secrets)
{
    auto key_sharer = relay_state.GetRelayByNumber(key_sharer_number);
    auto dead_relay = relay_state.GetRelayByNumber(dead_relay_number);

    vector<CBigNum> encrypted_key_sixteenths = GetEncryptedKeySixteenthsSentFromSharerToRelay(key_sharer, dead_relay);

    for (int i = 0; i < 4; i++)
    {
        uint32_t key_sixteenth_position = 4 * (uint32_t)key_quarter_position + i;

        Point public_key_sixteenth = key_sharer->PublicKeySixteenths()[key_sixteenth_position];

        CBigNum decrypted_key_sixteenth = encrypted_key_sixteenths[i] ^ Hash(shared_secrets[i]);

        if (Point(decrypted_key_sixteenth) != public_key_sixteenth)
            return false;

        data.keydata[public_key_sixteenth]["privkey"] = decrypted_key_sixteenth;
    }
    return true;
}

vector<CBigNum> RelayMessageHandler::GetEncryptedKeySixteenthsSentFromSharerToRelay(Relay *sharer, Relay *relay)
{
    KeyDistributionMessage key_distribution_message = data.GetMessage(sharer->key_distribution_message_hash);
    auto &encrypted_secrets = key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders;

    int64_t key_quarter_position = PositionOfEntryInVector(relay->number, sharer->key_quarter_holders);

    vector<CBigNum> encrypted_key_sixteenths;
    for (int64_t key_sixteenth_position = 4 * key_quarter_position;
            key_sixteenth_position < 4 + 4 * key_quarter_position; key_sixteenth_position++)
    {
        encrypted_key_sixteenths.push_back(encrypted_secrets[key_sixteenth_position]);
    }
    return encrypted_key_sixteenths;
}

void RelayMessageHandler::SendComplaintAboutFailedSecretRecovery(vector<uint160> recovery_message_hashes)
{

}