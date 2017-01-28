#include "SecretRecoveryFailureMessage.h"
#include "RelayState.h"

#include "log.h"
#define LOG_CATEGORY "SecretRecoveryFailureMessage.cpp"


Point SecretRecoveryFailureMessage::VerificationKey(Data data)
{
    Obituary obituary = data.GetMessage(obituary_hash);
    auto successor = data.relay_state->GetRelayByNumber(obituary.successor_number);
    if (successor == NULL)
        throw RelayStateException("SecretRecoveryFailureMessage::VerificationKey: no such relay");
    return successor->public_signing_key;
}

void SecretRecoveryFailureMessage::Populate(std::vector<uint160> recovery_message_hashes_, Data data)
{
    recovery_message_hashes = recovery_message_hashes_;
    SecretRecoveryMessage recovery_message = data.GetMessage(recovery_message_hashes[0]);

    obituary_hash = recovery_message.obituary_hash;

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

    auto array_of_shared_secrets = SecretRecoveryMessage::GetSharedSecrets(recovery_message_hashes, data);
    sum_of_decrypted_shared_secret_quarters = array_of_shared_secrets[failure_sharer_position][failure_key_part_position];
}

Relay *SecretRecoveryFailureMessage::GetDeadRelay(Data data)
{
    Obituary obituary = data.GetMessage(obituary_hash);
    return data.relay_state->GetRelayByNumber(obituary.dead_relay_number);
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
