#ifndef TELEPORT_ADDRESSPARTSECRET_H
#define TELEPORT_ADDRESSPARTSECRET_H


#include <src/crypto/uint256.h>
#include <array>
#include <src/crypto/point.h>
#include <test/teleport_tests/node/Data.h>
#include <test/teleport_tests/node/relays/structures/Relay.h>

#define SECRETS_PER_ADDRESS_PART 6


class AddressPartSecret
{
public:
    uint8_t curve{0};
    std::array<uint256, SECRETS_PER_ADDRESS_PART> encrypted_secrets;
    std::array<Point, SECRETS_PER_ADDRESS_PART> parts_of_address;
    uint160 encoding_credit_hash{0};
    uint160 relay_chooser{0};

    AddressPartSecret() = default;

    AddressPartSecret(uint8_t curve, uint160 encoding_credit_hash, uint160 relay_chooser, Data data, Relay *relay):
            curve(curve), encoding_credit_hash(encoding_credit_hash), relay_chooser(relay_chooser)
    {
        PopulateSecrets(data, relay);
    }


    IMPLEMENT_SERIALIZE
    (
        READWRITE(curve);
        READWRITE(encrypted_secrets);
        READWRITE(parts_of_address);
    );

    JSON(curve, encrypted_secrets, parts_of_address);

    void PopulateSecrets(Data data, Relay *relay)
    {
        Point pubkey(curve, 0);
        CBigNum private_key{0};
        for (uint32_t i = 0; i < SECRETS_PER_ADDRESS_PART; i++)
        {
            CBigNum secret;
            secret.Randomize(pubkey.Modulus());
            private_key += secret;
            parts_of_address[i] = Point(curve, secret);
            encrypted_secrets[i] = relay->EncryptSecret(secret);
            data.keydata[parts_of_address[i]]["privkey"] = secret;
            pubkey += parts_of_address[i];
        }
        data.keydata[pubkey]["privkey"] = private_key;
    }

    Point PubKey()
    {
        Point pubkey(parts_of_address[0].curve, 0);
        for (auto &point : parts_of_address)
            pubkey += point;
        return pubkey;
    }
};


#endif //TELEPORT_ADDRESSPARTSECRET_H
