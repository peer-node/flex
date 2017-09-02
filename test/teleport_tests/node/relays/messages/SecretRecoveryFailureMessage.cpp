#include "SecretRecoveryFailureMessage.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"

#include "log.h"
#define LOG_CATEGORY "SecretRecoveryFailureMessage.cpp"


Point SecretRecoveryFailureMessage::VerificationKey(Data data)
{
    auto successor = data.relay_state->GetRelayByNumber(successor_number);
    if (successor == NULL)
        throw RelayStateException("SecretRecoveryFailureMessage::VerificationKey: no such relay");
    return successor->public_signing_key;
}

void SecretRecoveryFailureMessage::Populate(std::array<uint160, 4> recovery_message_hashes_, Data data)
{
    recovery_message_hashes = recovery_message_hashes_;
    SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hashes[0]);
    dead_relay_number = recovery_message.dead_relay_number;
    successor_number = recovery_message.successor_number;

    PopulateDetailsOfFailedSharedSecret(data);
}

void SecretRecoveryFailureMessage::PopulateDetailsOfFailedSharedSecret(Data data)
{
    uint32_t failure_sharer_position{0}, failure_key_part_position{0};
    SecretRecoveryMessage::RecoverSecretsFromSecretRecoveryMessages(recovery_message_hashes,
                                                                    failure_sharer_position,
                                                                    failure_key_part_position, data);
    key_sharer_position = failure_sharer_position;
    shared_secret_quarter_position = failure_key_part_position;

    auto shared_secrets = SecretRecoveryMessage::GetSharedSecrets(recovery_message_hashes, data);
    sum_of_decrypted_shared_secret_quarters = shared_secrets[failure_sharer_position][failure_key_part_position];
}

Relay *SecretRecoveryFailureMessage::GetDeadRelay(Data data)
{
    return data.relay_state->GetRelayByNumber(dead_relay_number);
}

bool SecretRecoveryFailureMessage::ValidateSizes()
{
    return recovery_message_hashes.size() == 4;
}

bool SecretRecoveryFailureMessage::IsValid(Data data)
{
    if (not ValidateSizes())
        return false;

    SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hashes[0]);
    if (key_sharer_position >= recovery_message.key_quarter_sharers.size())
        return false;

    if (shared_secret_quarter_position >= 4)
        return false;;

    return true;
}

std::vector<Relay *> SecretRecoveryFailureMessage::GetQuarterHolders(Data data)
{
    std::vector<Relay *> quarter_holders;
    for (auto recovery_message_hash : recovery_message_hashes)
    {
        SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hash);
        quarter_holders.push_back(recovery_message.GetKeyQuarterHolder(data));
    }
    return quarter_holders;
}

Point SecretRecoveryFailureMessage::GetKeyTwentyFourth(Data data)
{
    auto key_sharer = GetKeySharer(data);
    auto key_quarter_position = GetKeyQuarterPosition(data);
    auto key_twentyfourth_position = 6 * key_quarter_position + shared_secret_quarter_position;
    return key_sharer->PublicKeyTwentyFourths()[key_twentyfourth_position];
}

uint64_t SecretRecoveryFailureMessage::GetKeyQuarterPosition(Data data)
{
    SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hashes[0]);
    return recovery_message.key_quarter_positions[key_sharer_position];
}

Relay *SecretRecoveryFailureMessage::GetKeySharer(Data data)
{
    SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hashes[0]);
    auto key_sharer_number = recovery_message.key_quarter_sharers[key_sharer_position];
    return data.relay_state->GetRelayByNumber(key_sharer_number);
}
