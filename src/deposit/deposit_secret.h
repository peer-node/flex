// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef TELEPORT_DEPOSIT_SECRET
#define TELEPORT_DEPOSIT_SECRET

#include "crypto/polynomial.h"
#include "crypto/teleportcrypto.h"
#include "relays/relaystate.h"

#define SECRETS_PER_ADDRESS_PART 4
#define ADDRESS_PART_SECRETS_NEEDED 3

#ifndef _PRODUCTION_BUILD

#define SECRETS_PER_DEPOSIT 3

#else

#define SECRETS_PER_DEPOSIT 12

#endif

#include "log.h"
#define LOG_CATEGORY "deposit_secret.h"

class AddressPartSecretDisclosure;

class AddressPartSecret
{
public:
    uint160 credit_hash;
    uint160 relay_chooser;
    uint160 relay_list_hash;
    std::vector<Point> coefficient_points;
    std::vector<Point> point_values;
    std::vector<CBigNum> secrets_xor_shared_secrets;

    std::vector<Point> relays;
    CBigNum privkey;

    std::vector<CBigNum> coefficients;
    std::vector<CBigNum> secrets;

    AddressPartSecret() { }

    AddressPartSecret(uint8_t curve,
                      uint160 credit_hash,
                      uint160 relay_chooser);

    Point PubKey()
    {
        return coefficient_points.size() > 0 ? coefficient_points[0]
                                             : Point(SECP256K1, 0);
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_hash);
        READWRITE(relay_chooser);
        READWRITE(relay_list_hash);
        READWRITE(coefficient_points);
        READWRITE(point_values);
        READWRITE(secrets_xor_shared_secrets);
    )

    string_t ToString() const;

    std::vector<Point> Relays();

    bool Validate();

    bool ValidateKeys();

    bool ValidateAndStoreMySecrets(std::vector<uint32_t>& bad_secrets);

    CBigNum RecoverPrivKey();

    int32_t RelayPosition(Point relay)
    {
        relays = Relays();
        for (uint32_t i = 0; i < relays.size(); i++)
            if (relays[i] == relay)
                return i;
        return -1;
    }
};


class AddressPartSecretDisclosure
{
public:
    uint160 credit_hash;
    uint160 relay_chooser;
    uint160 relay_list_hash;
    std::vector<CBigNum> secrets_xor_shared_secrets;

    std::vector<Point> relays;

    AddressPartSecretDisclosure() { }

    AddressPartSecretDisclosure(uint160 credit_hash,
                                AddressPartSecret& secret,
                                uint160 relay_chooser);

    std::vector<Point> Relays();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_hash);
        READWRITE(relay_chooser);
        READWRITE(relay_list_hash);
        READWRITE(secrets_xor_shared_secrets);
    )

    bool Validate();

    bool ValidatePurportedlyBadSecret(AddressPartSecret& secret,
                                      uint32_t position, 
                                      CBigNum secret_);

    bool ValidateAndStoreMySecrets(
        AddressPartSecret& secret,
        std::vector<uint32_t>& bad_disclosure_secrets,
        BigNumsInPositions& bad_original_secrets);
};



#endif