#include "RelayAdmissionHandler.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"
#include "RelayMessageHandler.h"

using std::vector;
using std::string;

#include "log.h"
#define LOG_CATEGORY "RelayAdmissionHandler.cpp"


RelayAdmissionHandler::RelayAdmissionHandler(Data data_, CreditSystem *credit_system, Calendar *calendar,
                                             RelayMessageHandler *handler):
    data(data_), relay_state(&handler->relay_state), credit_system(credit_system), calendar(calendar),
    relay_message_handler(handler)
{
    data.relay_state = relay_state;
}

void RelayAdmissionHandler::HandleRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    log_ << "RelayAdmissionHandler::HandleRelayJoinMessage\n";
    if (not ValidateRelayJoinMessage(relay_join_message))
    {
        log_ << "join message " << relay_join_message.GetHash160() << " failed validation\n";
        relay_message_handler->RejectMessage(relay_join_message.GetHash160());
        return;
    }

    if (relay_message_handler->mode != BLOCK_VALIDATION and not MinedCreditMessageHashIsInMainChain(relay_join_message))
    {
        log_ << "mined credit message hash is not in main chain\n";
        QueueJoinMessageBehindMinedCreditMessageHash(relay_join_message);
        return;
    }
    log_ << "accepting join " << relay_join_message.GetHash160() << "\n";
    AcceptRelayJoinMessage(relay_join_message);
}

bool Fail(std::string message)
{
    log_ << message << "\n";
    return false;
}

bool RelayAdmissionHandler::ValidateRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    if (not relay_join_message.ValidateSizes())
        return Fail("bad sizes");
    if (relay_state->MinedCreditMessageHashIsAlreadyBeingUsed(relay_join_message.mined_credit_message_hash))
        return Fail("mined credit message hash already being used");
    if (not relay_join_message.VerifySignature(data))
        return Fail("bad signature");
    if (MinedCreditMessageIsNewerOrMoreThanThreeBatchesOlderThanLatest(relay_join_message.mined_credit_message_hash))
        return Fail("mined credit message is newer or more than three batches older than latest");
    return true;
}

bool
RelayAdmissionHandler::MinedCreditMessageIsNewerOrMoreThanThreeBatchesOlderThanLatest(uint160 mined_credit_message_hash)
{
    MinedCreditMessage latest_msg = data.msgdata[relay_state->latest_mined_credit_message_hash]["msg"];
    MinedCreditMessage given_msg = data.msgdata[mined_credit_message_hash]["msg"];

    log_ << "latest batch number: " << latest_msg.mined_credit.network_state.batch_number
         << " this: " << given_msg.mined_credit.network_state.batch_number << "\n";
    return given_msg.mined_credit.network_state.batch_number > latest_msg.mined_credit.network_state.batch_number or
           given_msg.mined_credit.network_state.batch_number + 3 < latest_msg.mined_credit.network_state.batch_number;
}

bool RelayAdmissionHandler::MinedCreditMessageHashIsInMainChain(RelayJoinMessage relay_join_message)
{
    return credit_system->IsInMainChain(relay_join_message.mined_credit_message_hash);
}

void RelayAdmissionHandler::AcceptRelayJoinMessage(RelayJoinMessage join_message)
{
    relay_state->ProcessRelayJoinMessage(join_message);
    relay_message_handler->AcceptForEncodingInChainIfLive(join_message.GetHash160());
}

void RelayAdmissionHandler::HandleKeyDistributionMessage(KeyDistributionMessage key_distribution_message)
{
    if (not ValidateKeyDistributionMessage(key_distribution_message))
    {
        relay_message_handler->RejectMessage(key_distribution_message.GetHash160());
        return;
    }

    AcceptKeyDistributionMessage(key_distribution_message);
}

void RelayAdmissionHandler::AcceptKeyDistributionMessage(KeyDistributionMessage& key_distribution_message)
{
    relay_state->ProcessKeyDistributionMessage(key_distribution_message);
    relay_message_handler->AcceptForEncodingInChainIfLive(key_distribution_message.GetHash160());
    StoreKeyDistributionSecretsAndSendComplaints(key_distribution_message);
    if (relay_message_handler->mode == LIVE)
        scheduler.Schedule("key_distribution",
                           key_distribution_message.GetHash160(), GetTimeMicros() + RESPONSE_WAIT_TIME);
}

void RelayAdmissionHandler::HandleKeyDistributionMessageAfterDuration(uint160 key_distribution_message_hash)
{
    KeyDistributionMessage key_distribution_message = data.GetMessage(key_distribution_message_hash);
    auto key_sharer = relay_state->GetRelayByNumber(key_distribution_message.relay_number);
    if (key_sharer == NULL)
        return;

    if (key_sharer->hashes.key_distribution_complaint_hashes.size() > 0)
        return;
    DurationWithoutResponse duration;
    duration.message_hash = key_distribution_message_hash;

    relay_state->ProcessDurationWithoutResponse(duration, data);
    relay_message_handler->AcceptForEncodingInChainIfLive(duration.GetHash160());
}

bool RelayAdmissionHandler::ValidateKeyDistributionMessage(KeyDistributionMessage key_distribution_message)
{
    if (not key_distribution_message.ValidateSizes())
        return false;
    if (not key_distribution_message.VerifySignature(data))
        return false;
    if (not key_distribution_message.VerifyRelayNumber(data))
        return false;;
    return true;
}

void RelayAdmissionHandler::StoreKeyDistributionSecretsAndSendComplaints(KeyDistributionMessage key_distribution_message)
{
    Relay *relay = relay_state->GetRelayByNumber(key_distribution_message.relay_number);

    RecoverSecretsAndSendKeyDistributionComplaints(
            relay,
            relay_state->GetKeyQuarterHoldersAsListOf24RelayNumbers(relay->number),
            key_distribution_message.GetHash160(),
            key_distribution_message.EncryptedKeyTwentyFourths());
}

void RelayAdmissionHandler::RecoverSecretsAndSendKeyDistributionComplaints(Relay *relay,
                                                                           vector<uint64_t> recipients,
                                                                           uint160 key_distribution_message_hash,
                                                                           std::array<uint256, 24> encrypted_secrets)
{
    auto public_key_twentyfourths = relay->PublicKeyTwentyFourths();
    for (uint32_t position = 0; position < recipients.size(); position++)
    {
        Relay *recipient = data.relay_state->GetRelayByNumber(recipients[position]);

        if (not data.keydata[recipient->public_signing_key].HasProperty("privkey"))
            continue;

        bool ok = RecoverAndStoreSecret(recipient, encrypted_secrets[position], public_key_twentyfourths[position])
                  and relay->public_key_set.VerifyRowOfGeneratedPoints(position, data.keydata);

        if (not ok and relay_message_handler->mode == LIVE and send_key_distribution_complaints)
            SendKeyDistributionComplaint(key_distribution_message_hash, position);
    }
}

bool RelayAdmissionHandler::RecoverAndStoreSecret(Relay *recipient, uint256 &encrypted_secret,
                                                  Point &point_corresponding_to_secret)
{
    CBigNum decrypted_secret = recipient->DecryptSecret(encrypted_secret, point_corresponding_to_secret, data);

    if (decrypted_secret == 0)
        return false;

    data.keydata[point_corresponding_to_secret]["privkey"] = decrypted_secret;
    return true;
}

void RelayAdmissionHandler::SendKeyDistributionComplaint(uint160 key_distribution_message_hash,
                                                         uint32_t position_of_bad_secret)
{
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message_hash, position_of_bad_secret, data);
    complaint.Sign(data);
    data.StoreMessage(complaint);
    relay_message_handler->Broadcast(complaint);
    relay_message_handler->Handle(complaint, NULL);
}

void RelayAdmissionHandler::HandleKeyDistributionComplaint(KeyDistributionComplaint complaint)
{
    if (complaint.GetSecretSender(data) == NULL) // might have been ejected due to earlier complaint
        return;
    if (not ValidateKeyDistributionComplaint(complaint))
    {
        relay_message_handler->RejectMessage(complaint.GetHash160());
        return;
    }
    AcceptKeyDistributionComplaint(complaint);
}

void RelayAdmissionHandler::AcceptKeyDistributionComplaint(KeyDistributionComplaint &complaint)
{
    relay_state->ProcessKeyDistributionComplaint(complaint, data);
    relay_message_handler->AcceptForEncodingInChainIfLive(complaint.GetHash160());
    relay_message_handler->HandleNewlyDeadRelays();
}

bool RelayAdmissionHandler::ValidateKeyDistributionComplaint(KeyDistributionComplaint complaint)
{
    return complaint.IsValid(data) and complaint.VerifySignature(data);
}

bool
RelayAdmissionHandler::ValidateDurationWithoutResponseAfterKeyDistributionMessage(DurationWithoutResponse &duration)
{
    KeyDistributionMessage key_distribution_message = data.GetMessage(duration.message_hash);
    auto key_sharer = relay_state->GetRelayByNumber(key_distribution_message.relay_number);
    if (key_sharer == NULL)
        return false;
    return key_sharer->hashes.key_distribution_complaint_hashes.size() == 0;
}

void RelayAdmissionHandler::QueueJoinMessageBehindMinedCreditMessageHash(RelayJoinMessage relay_join_message)
{
    data.msgdata[relay_join_message.mined_credit_message_hash]["queued_join"] = relay_join_message.GetHash160();
}
