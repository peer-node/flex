#ifndef TELEPORT_DEPOSITADDRESSPARTMESSAGE_H
#define TELEPORT_DEPOSITADDRESSPARTMESSAGE_H


#include <src/define.h>
#include <src/crypto/point.h>
#include <test/teleport_tests/node/credit/messages/MinedCreditMessage.h>
#include <test/teleport_tests/node/relays/structures/RelayState.h>
#include "DepositAddressRequest.h"
#include "AddressPartSecret.h"

#define PARTS_PER_DEPOSIT_ADDRESS 10

class DepositAddressPartMessage
{
public:
    uint160 address_request_hash{0};
    uint160 relay_list_hash{0};
    uint32_t position{0};
    uint160 encoding_message_hash{0};
    uint160 relay_chooser{0};
    AddressPartSecret address_part_secret;
    Signature signature;

    DepositAddressPartMessage() = default;

    DepositAddressPartMessage(uint160 address_request_hash,
                              uint160 encoding_message_hash,
                              uint32_t position,
                              Data data,
                              Relay *relay):
            encoding_message_hash(encoding_message_hash),
            address_request_hash(address_request_hash),
            position(position),
            relay_chooser((encoding_message_hash * (1 + position)) xor address_request_hash),
            address_part_secret(GetRequest(data).curve, encoding_message_hash, relay_chooser, data, relay)
    { }

    static string_t Type() { return string_t("deposit_part"); }

    IMPLEMENT_SERIALIZE
    (
            READWRITE(address_request_hash);
            READWRITE(relay_list_hash);
            READWRITE(position);
            READWRITE(encoding_message_hash);
            READWRITE(relay_chooser);
            READWRITE(address_part_secret);
            READWRITE(signature);
    );

    JSON(address_request_hash, relay_list_hash, position, encoding_message_hash,
         relay_chooser, address_part_secret, signature);

    DEPENDENCIES(address_request_hash);
    
    bool ValidateEnclosedData()
    {
        return position < PARTS_PER_DEPOSIT_ADDRESS and
                relay_chooser == ((encoding_message_hash * (1 + position)) xor address_request_hash);
    }

    uint160 EncodedRequestIdentifier()
    {
        return address_request_hash + encoding_message_hash;
    }

    Point PubKey()
    {
        return address_part_secret.PubKey();
    }
    DepositAddressRequest GetRequest(Data data)
    {
        return data.GetMessage(address_request_hash);
    }

    CBigNum GetSecret(uint32_t position, Data data)
    {
        Point point_value = address_part_secret.parts_of_address[position];
        return data.keydata[point_value]["privkey"];
    }

    Point GetPoint(uint32_t position)
    {
        return address_part_secret.parts_of_address[position];
    }

    bool CheckSecretAtPosition(CBigNum &secret, uint32_t position, Data data)
    {
        return Point(GetRequest(data).curve, secret) == address_part_secret.parts_of_address[position];
    }

    MinedCreditMessage MinedCreditMessageInWhichRequestWasEncoded(Data data)
    {
        log_ << "MinedCreditMessageInWhichRequestWasEncoded: " << encoding_message_hash << "\n";
        return data.GetMessage(encoding_message_hash);
    }

    RelayState RelayStateFromWhichRelaysAreChosen(Data data)
    {
        auto msg = MinedCreditMessageInWhichRequestWasEncoded(data);
        return data.GetRelayState(msg.mined_credit.network_state.relay_state_hash);
    }

    std::vector<uint64_t> RelaysForAddressRequest(Data data)
    {
        auto relay_state = RelayStateFromWhichRelaysAreChosen(data);
        return relay_state.ChooseRelaysForDepositAddressRequest(relay_chooser, PARTS_PER_DEPOSIT_ADDRESS);
    }

    uint64_t RelayNumber(Data data)
    {
        std::vector<uint64_t> relay_numbers = RelaysForAddressRequest(data);
        if (position > relay_numbers.size())
            return 0;
        return relay_numbers[position];
    }

    Point VerificationKey(Data data)
    {
        log_ << "VerificationKey()\n";
        auto relay_state = RelayStateFromWhichRelaysAreChosen(data);
        log_ << "relay state size is " << relay_state.relays.size() << "\n";
        log_ << "relay number is " << RelayNumber(data) << "\n";
        auto relay = relay_state.GetRelayByNumber(RelayNumber(data));
        log_ << "null relay: " << (relay == NULL) << "\n";
        if (relay == NULL)
            return Point(SECP256K1, 0);
        return relay->public_signing_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_DEPOSITADDRESSPARTMESSAGE_H
