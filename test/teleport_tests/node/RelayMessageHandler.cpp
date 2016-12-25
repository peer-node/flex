#include "RelayMessageHandler.h"

#include "log.h"
#define LOG_CATEGORY "RelayMessageHandler.cpp"

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
    SendKeyDistributionComplaintsIfAppropriate(key_distribution_message);
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
    {
        log_ << "bad signature\n";
        return false;;
    }
    return true;
}

void RelayMessageHandler::SendKeyDistributionComplaintsIfAppropriate(KeyDistributionMessage key_distribution_message)
{
    Relay *relay = relay_state.GetRelayByNumber(key_distribution_message.relay_number);

    SendKeyDistributionComplaints(relay, KEY_DISTRIBUTION_COMPLAINT_KEY_QUARTERS,
                                  relay_state.GetKeyQuarterHoldersAsListOf16Recipients(relay->number),
                                  key_distribution_message.GetHash160(),
                                  key_distribution_message.key_sixteenths_encrypted_for_key_quarter_holders);

    SendKeyDistributionComplaints(relay, KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS,
                                  relay_state.GetKeySixteenthHoldersAsListOf16Recipients(relay->number, 1),
                                  key_distribution_message.GetHash160(),
                                  key_distribution_message.key_sixteenths_encrypted_for_first_set_of_key_sixteenth_holders);

    SendKeyDistributionComplaints(relay, KEY_DISTRIBUTION_COMPLAINT_SECOND_KEY_SIXTEENTHS,
                                  relay_state.GetKeySixteenthHoldersAsListOf16Recipients(relay->number, 2),
                                  key_distribution_message.GetHash160(),
                                  key_distribution_message.key_sixteenths_encrypted_for_second_set_of_key_sixteenth_holders);
}

void RelayMessageHandler::SendKeyDistributionComplaints(Relay *relay, uint64_t set_of_secrets,
                                                        std::vector<Point> recipients,
                                                        uint160 key_distribution_message_hash,
                                                        std::vector<CBigNum> encrypted_secrets)
{
    for (uint32_t position = 0; position < recipients.size(); position++)
    {
        if (not data.keydata[recipients[position]].HasProperty("privkey"))
            continue;

        if (not RecoverAndStoreSecret(recipients[position],
                                      encrypted_secrets[position],
                                      relay->public_key_sixteenths[position]))
            SendKeyDistributionComplaint(key_distribution_message_hash, set_of_secrets, position);
    }
}

bool RelayMessageHandler::RecoverAndStoreSecret(Point &recipient_public_key, CBigNum &encrypted_secret,
                                                Point &point_corresponding_to_secret)
{
    CBigNum private_key = data.keydata[recipient_public_key]["privkey"];

    CBigNum decrypted_secret = encrypted_secret ^ Hash(private_key * point_corresponding_to_secret);

    if (Point(SECP256K1, decrypted_secret) != point_corresponding_to_secret)
        return false;

    data.keydata[point_corresponding_to_secret]["privkey"] = decrypted_secret;
    return true;
}

void RelayMessageHandler::SendKeyDistributionComplaint(uint160 key_distribution_message_hash, uint64_t set_of_secrets,
                                                       uint32_t position_of_bad_secret)
{
    KeyDistributionComplaint complaint(key_distribution_message_hash, set_of_secrets, position_of_bad_secret);
    complaint.Sign(data);
    Broadcast(complaint);
}


