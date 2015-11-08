// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef FLEX_SUCCESSION_SECRET
#define FLEX_SUCCESSION_SECRET

#include "crypto/polynomial.h"
#include "crypto/flexcrypto.h"
#include "relays/relaystate.h"

#include "log.h"
#define LOG_CATEGORY "successionsecret.h"

#define NUM_SUCCESSION_SECRETS 6
#define SUCCESSION_SECRETS_NEEDED 5


#ifndef _PRODUCTION_BUILD
#define INHERITANCE_START 9
#else
#define INHERITANCE_START 37
#endif


class DistributedSuccessionSecret
{
public:
    uint160 credit_hash;
    uint32_t relay_number;
    uint160 relay_list_hash;
    std::vector<Point> coefficient_points;
    std::vector<Point> point_values;
    std::vector<CBigNum> secrets_xor_shared_secrets;

    std::vector<Point> relays;
    CBigNum privkey;

    std::vector<CBigNum> coefficients;

    DistributedSuccessionSecret(): relay_number(0) { }
    ~DistributedSuccessionSecret() { }

    DistributedSuccessionSecret(uint160 credit_hash,
                                uint32_t relay_number):
        credit_hash(credit_hash),
        relay_number(relay_number)
    {
        log_ << "DistributedSuccessionSecret()\n";
        relays = Relays();
        relay_list_hash = PointListHash(relays);
        std::vector<CBigNum> secrets;
        Point pubkey;

        generate_key_with_secret_parts(SECP256K1,
                                       pubkey,
                                       secrets,
                                       coefficients,
                                       relays.size(),
                                       &privkey);
        coefficient_points = publics_from_privates(SECP256K1, coefficients);
        point_values = publics_from_privates(SECP256K1, secrets);

        keydata[pubkey]["privkey"] = privkey;
        log_ << "relays are: " << relays << "\n";
        for (uint32_t i = 0; i < relays.size(); i++)
        {
#ifndef _PRODUCTION_BUILD
            log_ << "adding secret " << secrets[i]
                 << " for " << relays[i] << "\n";
#endif
            CBigNum shared_secret = Hash(secrets[i] * relays[i]);
            CBigNum secret_xor_shared_secret = secrets[i] ^ shared_secret;
            secrets_xor_shared_secrets.push_back(secret_xor_shared_secret);
            keydata[Point(SECP256K1, secrets[i])]["privkey"] = secrets[i];
        }
    }
    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n============= DistributedSuccessionSecret ===========\n"
           << "== Credit Hash: " << credit_hash.ToString() << "\n"
           << "== Relay Number: " << relay_number << "\n"
           << "== Relay List Hash: " << relay_list_hash.ToString() << "\n"
           << "== Coefficient Points: \n";
        foreach_(Point point, coefficient_points)
            ss << "== " << point.ToString() << "\n";
        ss << "==\n"
           << "== Point Values: \n";
        foreach_(Point point, point_values)
            ss << "== " << point.ToString() << "\n";
        ss << "==\n"
           << "== Secrets xor Shared Secrets: \n";
        foreach_(CBigNum secret_xor_shared_secret, secrets_xor_shared_secrets)
            ss << "== " << secret_xor_shared_secret.ToString() << "\n";
        ss << "=========== End DistributedSuccessionSecret =========\n";
        return ss.str(); 
    }
    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_hash);
        READWRITE(relay_number);
        READWRITE(relay_list_hash);
        READWRITE(coefficient_points);
        READWRITE(point_values);
        READWRITE(secrets_xor_shared_secrets);
    )

    std::vector<Point> Relays();

    int64_t Position(Point executor)
    {
        std::vector<Point> relays = Relays();
        for (uint32_t i = 0; i < relays.size(); i++)
        {
            if (relays[i] == executor)
                return i;
        }
        return -1;
    }

    bool Validate()
    {
        if (coefficient_points.size() != SUCCESSION_SECRETS_NEEDED)
        {
            log_ << "Wrong number of coefficients: "
                 << coefficient_points.size() << " vs "
                 << SUCCESSION_SECRETS_NEEDED << "\n";
            return false;
        }
        if (point_values.size() != NUM_SUCCESSION_SECRETS)
        {
            log_ << "Wrong number of point values: "
                 << point_values.size() << " vs "
                 << SUCCESSION_SECRETS_NEEDED << "\n";
            return false;
        }
        if (secrets_xor_shared_secrets.size() != NUM_SUCCESSION_SECRETS)
        {
            log_ << "Wrong number of secrets: "
                 << secrets_xor_shared_secrets.size() << " vs "
                 << NUM_SUCCESSION_SECRETS << "\n";
            return false;
        }

        return PointListHash(Relays()) == relay_list_hash && ValidateKeys();
    }

    bool ValidateKeys()
    {
        uint32_t n = NUM_SUCCESSION_SECRETS;
        std::vector<CBigNum> one_two_three_etc = PositiveIntegers(n);
        std::vector<Point> points;
        PointPolynomial ppoly(SECP256K1, coefficient_points);

        for (uint32_t i = 0; i < n; i++) 
        {
            Point evaluation = ppoly.evaluate(one_two_three_etc[i]);
            if (evaluation != point_values[i])
                return false;
            points.push_back(evaluation);
        }

        Point pubkey = PubKey();

        return confirm_public_key_with_N_polynomial_point_values(pubkey, 
                                                                 points, n);
    }

    bool ValidateAndStoreMySecretPieces(std::vector<uint32_t> failures)
    {
        relays = Relays();
        bool ok = true;
        for (uint32_t i = 0; i < relays.size(); i++)
        {
            if (!keydata[relays[i]].HasProperty("privkey"))
                continue;
            
            CBigNum privkey = keydata[relays[i]]["privkey"];
            
            CBigNum shared_secret = Hash(privkey * point_values[i]);
            CBigNum secret = shared_secret ^ secrets_xor_shared_secrets[i];

            if (Point(SECP256K1, secret) == point_values[i])
            {
                keydata[point_values[i]]["privkey"] = secret;
                continue;
            }
            else
            {
                ok = false;
                failures.push_back(i);
                continue;
            }
        }
        return ok;
    }

    Point PubKey()
    {
        return coefficient_points[0];
    }

    CBigNum RecoverPrivKey()
    {
        CBigNum privkey;
        std::vector<CBigNum> secrets;
        bool cheated;
        std::vector<int32_t> cheaters;
        log_ << "RecoverPrivKey()\n";
        std::vector<Point> commitments;

        for (uint32_t i = 0; i < point_values.size(); i++)
        {
            CBigNum secret = keydata[point_values[i]]["privkey"];
#ifndef _PRODUCTION_BUILD
            log_ << "adding secret " << secret << "\n";
#endif
            secrets.push_back(secret);
            commitments.push_back(point_values[i]);
        }

        generate_private_key_from_N_secret_parts(SECP256K1,
                                                 privkey,
                                                 secrets,
                                                 commitments,
                                                 secrets.size(),
                                                 cheated,
                                                 cheaters);
        return privkey;
    }
    
    int32_t RelayPosition(Point relay)
    {
        relays = Relays();
        for (uint32_t i = 0; i < relays.size(); i++)
            if (relays[i] == relay)
                return i;
        return -1;
    }
};

#endif