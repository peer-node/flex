#ifndef TELEPORT_ADDRESSPARTSECRETDISCLOSURE_H
#define TELEPORT_ADDRESSPARTSECRETDISCLOSURE_H

#include "AddressPartSecret.h"
#include "../../relays/structures/RelayState.h"

#define AUDITORS_PER_SECRET 2

class AddressPartSecretDisclosure
{
public:
    std::array<std::array<uint256, SECRETS_PER_ADDRESS_PART>, AUDITORS_PER_SECRET> secrets_encrypted_for_auditors;
    std::array<std::array<uint64_t, SECRETS_PER_ADDRESS_PART>, AUDITORS_PER_SECRET> auditor_numbers;
    uint160 post_encoding_message_hash{0};
    uint160 relay_chooser{0};

    IMPLEMENT_SERIALIZE
    (
        READWRITE(secrets_encrypted_for_auditors);
        READWRITE(auditor_numbers);
        READWRITE(post_encoding_message_hash);
        READWRITE(relay_chooser);
    );

    AddressPartSecretDisclosure() = default;

    AddressPartSecretDisclosure(uint160 post_encoding_message_hash, AddressPartSecret address_part_secret,
                                uint160 relay_chooser, Data data):
            post_encoding_message_hash(post_encoding_message_hash), relay_chooser(relay_chooser)
    {
        auditor_numbers = DetermineAuditors(data);
        EncryptSecretsForAuditors(address_part_secret, data);
    }

    void EncryptSecretsForAuditors(AddressPartSecret address_part_secret, Data data)
    {
        auto relay_state = RelayStateFromPostEncodingMessage(data);
        for (uint32_t first_or_second = 0; first_or_second < AUDITORS_PER_SECRET; first_or_second++)
            for (uint32_t position = 0; position < SECRETS_PER_ADDRESS_PART; position++)
            {
                Point pubkey_of_secret = address_part_secret.parts_of_address[position];
                CBigNum secret = data.keydata[pubkey_of_secret]["privkey"];
                Relay *auditor = relay_state.GetRelayByNumber(auditor_numbers[first_or_second][position]);
                secrets_encrypted_for_auditors[first_or_second][position] = auditor->EncryptSecret(secret);
            }
    }

    MinedCreditMessage PostEncodingMessage(Data data)
    {
        return data.GetMessage(post_encoding_message_hash);
    }

    RelayState RelayStateFromPostEncodingMessage(Data data)
    {
        auto msg = PostEncodingMessage(data);
        return data.GetRelayState(msg.mined_credit.network_state.relay_state_hash);
    }

    std::array<std::array<uint64_t, SECRETS_PER_ADDRESS_PART>, AUDITORS_PER_SECRET> DetermineAuditors(Data data)
    {
        std::array<std::array<uint64_t, SECRETS_PER_ADDRESS_PART>, AUDITORS_PER_SECRET> auditors;

        auto state = RelayStateFromPostEncodingMessage(data);
        auto relay_numbers = state.ChooseAuditorsForDepositAddressPart(relay_chooser,
                                                                       AUDITORS_PER_SECRET * SECRETS_PER_ADDRESS_PART);

        for (uint32_t first_or_second = 0; first_or_second < AUDITORS_PER_SECRET; first_or_second++)
            for (uint32_t position = 0; position < SECRETS_PER_ADDRESS_PART; position++)
                auditors[first_or_second][position] = relay_numbers[position + first_or_second * SECRETS_PER_ADDRESS_PART];

        return auditors;
    }

    bool ValidateAuditors(Data data)
    {
        return auditor_numbers == DetermineAuditors(data);
    }
};


#endif //TELEPORT_ADDRESSPARTSECRETDISCLOSURE_H
