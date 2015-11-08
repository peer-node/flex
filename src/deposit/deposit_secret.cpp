// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#include "flexnode/flexnode.h"

#include "log.h"
#define LOG_CATEGORY "deposit_secret.cpp"


/**********************
 * AddressPartSecret
 */

    AddressPartSecret::AddressPartSecret(uint8_t curve,
                                         uint160 credit_hash,
                                         uint160 relay_chooser):
        credit_hash(credit_hash),
        relay_chooser(relay_chooser)
    {
        relays = Relays();
        relay_list_hash = PointListHash(relays);
        std::vector<CBigNum> secrets;
        Point pubkey;

        generate_key_with_secret_parts(curve,
                                       pubkey,
                                       secrets,
                                       coefficients,
                                       relays.size(),
                                       &privkey);

#ifndef _PRODUCTION_BUILD
        log_ << "AddressPartSecret(): privkey is "
             << privkey << " and pubkey is " << pubkey << "\n";
#endif

        coefficient_points = publics_from_privates(curve, coefficients); 
        point_values = publics_from_privates(curve, secrets);
        keydata[pubkey]["privkey"] = privkey;

        for (uint32_t i = 0; i < relays.size(); i++)
        {
            keydata[Point(curve, secrets[i])]["privkey"] = secrets[i];
            
            CBigNum shared_secret = Hash(relays[i] * secrets[i]);
            secrets_xor_shared_secrets.push_back(secrets[i] ^ shared_secret);
        }
    }

    string_t AddressPartSecret::ToString() const
    {
        std::stringstream ss;

        ss << "\n============= AddressPartSecret ===========\n"
           << "== Credit Hash: " << credit_hash.ToString() << "\n"
           << "== Relay Chooser: " << relay_chooser.ToString() << "\n"
           << "== Relay List Hash: " << relay_list_hash.ToString() << "\n"
           << "== Coefficient points: \n";
        foreach_(Point point, coefficient_points)
        ss << "== " << point.ToString() << "\n";
        ss << "==\n"
           << "== Point values: \n";
        foreach_(Point point, point_values)
        ss << "== " << point.ToString() << "\n";
        ss << "==\n"
           << "== Secrets xor Shared Secrets: \n";
        foreach_(CBigNum secret_xor_shared_secret, 
                 secrets_xor_shared_secrets)
        ss << "== " << secret_xor_shared_secret.ToString() << "\n";
        ss << "==\n"
           << "=========== End AddressPartSecret =========\n";
        return ss.str(); 
    }

    std::vector<Point> AddressPartSecret::Relays()
    {
        if (relays.size() == 0)
        {
            log_ << "AddressPartSecret::Relays: credit hash is "
                 << credit_hash << "\n";
            RelayState state = GetRelayState(credit_hash);
            relays = state.ChooseRelays(relay_chooser,
                                        SECRETS_PER_ADDRESS_PART);
        }
        return relays;
    }

    bool AddressPartSecret::Validate()
    {
        if (coefficient_points.size() != ADDRESS_PART_SECRETS_NEEDED ||
            point_values.size() != SECRETS_PER_ADDRESS_PART ||
            secrets_xor_shared_secrets.size() != SECRETS_PER_ADDRESS_PART)
            return false;

        relays = Relays();

        log_ << "AddressPartSecret::Validate():\n"
             << "relays = " << relays
             << "computed relay list hash: " << PointListHash(relays)
             << "reported relay list hash: " << relay_list_hash << "\n";

        log_ << "AddressPartSecret::Validate():\n"
             << "ValidateKeys(): " 
             << ValidateKeys() << "\n";

        return PointListHash(relays) == relay_list_hash && ValidateKeys();
    }

    bool AddressPartSecret::ValidateKeys()
    {
        Point pubkey = PubKey();
        uint32_t n = SECRETS_PER_ADDRESS_PART;
        std::vector<CBigNum> one_two_three_etc = PositiveIntegers(n);
        PointPolynomial ppoly(pubkey.curve, coefficient_points);

        for (uint32_t i = 0; i < n; i++) 
        {
            Point evaluation = ppoly.evaluate(one_two_three_etc[i]);
            if (evaluation != point_values[i])
            {
                log_ << "ValidateKeys: polynomial evaluation failed:\n"
                     << evaluation << " vs " << point_values[i] << "\n";
                return false;
            }
            else
            {
                log_ << "ValidateKeys: polynomial evaluation succeeded:\n"
                     << evaluation << " vs " << point_values[i] << "\n";
            }
        }

        log_ << "pubkey is " << pubkey << "\n"
             << "point_values is " << point_values << "\n"
             << "n is " << n << "\n";

        bool result = confirm_public_key_with_N_polynomial_point_values(
                        pubkey, point_values, n);
        log_ << "pubkey confirmation: " << result << "\n";
        return result;
    }

    bool AddressPartSecret::ValidateAndStoreMySecrets(
        std::vector<uint32_t>& bad_secrets)
    {
        relays = Relays();
        bool ok = true;
        for (uint32_t i = 0; i < relays.size(); i++)
        {
            if (!keydata[relays[i]].HasProperty("privkey"))
                continue;
    
            CBigNum privkey = keydata[relays[i]]["privkey"];
            CBigNum secret;

            CBigNum shared_secret = Hash(privkey * point_values[i]);

            secret = shared_secret ^ secrets_xor_shared_secrets[i];

            if (Point(PubKey().curve, secret) == point_values[i])
            {
                keydata[point_values[i]]["privkey"] = secret;
            }
            else
            {
                bad_secrets.push_back(i);
                ok = false;
            }
        }
        return ok;
    }

    CBigNum AddressPartSecret::RecoverPrivKey()
    {
        secrets.resize(0);
        bool cheated;
        std::vector<int32_t> cheaters;
        log_ << "RecoverPrivKey()\n";

        foreach_(Point point_value, point_values)
        {
            CBigNum secret = keydata[point_value]["privkey"];
            secrets.push_back(secret);
        }
        
        generate_private_key_from_N_secret_parts(PubKey().curve,
                                                 privkey,
                                                 secrets,
                                                 point_values,
                                                 secrets.size(),
                                                 cheated,
                                                 cheaters);
        return privkey;
    }

/*
 * AddressPartSecret
 **********************/


/********************************
 * AddressPartSecretDisclosure
 */

    AddressPartSecretDisclosure::AddressPartSecretDisclosure(
                                uint160 credit_hash,
                                AddressPartSecret& secret,
                                uint160 relay_chooser):
        credit_hash(secret.credit_hash),
        relay_chooser(relay_chooser)
    {
        std::vector<Point> relays = Relays();
        relay_list_hash = PointListHash(relays);

        for (uint32_t i = 0; i < relays.size(); i++)
        {
            Point point_value = secret.point_values[i];

            CBigNum secret = keydata[point_value]["privkey"];

            log_ << "AddressPartSecretDisclosure(): " << i
                 << " pubkey is " << point_value 
#ifndef _PRODUCTION_BUILD
                 << " and secret is " << secret 
#endif
                 << "\n";
            
            CBigNum shared_secret = Hash(secret * relays[i]);
            secrets_xor_shared_secrets.push_back(secret ^ shared_secret);
        }
    }

    std::vector<Point> AddressPartSecretDisclosure::Relays()
    {

        if (relays.size() == 0)
        {
            log_ << "AddressPartSecretDisclosure::Relays: credit hash is "
                 << credit_hash << "\n";
            RelayState state = GetRelayState(credit_hash);
            relays = state.ChooseRelays(relay_chooser,
                                        SECRETS_PER_ADDRESS_PART);
        }
        return relays;
    }

    bool AddressPartSecretDisclosure::Validate()
    {
        if (PointListHash(Relays()) != relay_list_hash)
        {
            log_ << "AddressPartSecretDisclosure::Validate(): "
                 << "bad relays: " << PointListHash(Relays())
                 << " != " << relay_list_hash << "\n";
            return false;
        }
        if (secrets_xor_shared_secrets.size() != SECRETS_PER_ADDRESS_PART)
        {
            log_ << "AddressPartSecretDisclosure::Validate(): "
                 << "wrong number of secrets: "
                 << secrets_xor_shared_secrets.size() 
                 << " vs " << SECRETS_PER_ADDRESS_PART << "\n";
            return false;
        }
        return true;
    }

    bool AddressPartSecretDisclosure::ValidateAndStoreMySecrets(
        AddressPartSecret& secret,
        std::vector<uint32_t>& bad_disclosure_secrets,
        BigNumsInPositions& bad_original_secrets)
    {
        relays = Relays();
        log_ << "ValidateAndStoreMySecrets: relays are " << relays << "\n";
        bool ok = true;
        std::vector<Point> original_relays = secret.Relays();
        log_ << "ValidateAndStoreMySecrets: original_relays are " 
             << original_relays << "\n";

        for (uint32_t i = 0; i < relays.size(); i++)
        {
            if (!keydata[relays[i]].HasProperty("privkey"))
                continue;
            
            CBigNum privkey = keydata[relays[i]]["privkey"];
            CBigNum secret_, shared_secret;
            
            shared_secret = Hash(privkey * secret.point_values[i]);

            secret_ = shared_secret ^ secrets_xor_shared_secrets[i];

            if (Point(secret.PubKey().curve, secret_) != secret.point_values[i])
            {
                bad_disclosure_secrets.push_back(i);
                continue;
            }

            keydata[secret.point_values[i]]["privkey"] = secret_;
            CBigNum original_shared_secret;
            original_shared_secret = Hash(secret_ * original_relays[i]);
            if ((secret_ ^ original_shared_secret) !=
                secret.secrets_xor_shared_secrets[i])
            {
                bad_original_secrets.push_back(std::make_pair(i, secret_));
                continue;
            }
        }
        return ok;
    }

    bool AddressPartSecretDisclosure::ValidatePurportedlyBadSecret(
        AddressPartSecret& secret,
        uint32_t position, 
        CBigNum secret_)
    {
        if (Point(secret.PubKey().curve, secret_)
            != secret.point_values[position])
            return false;
        std::vector<Point> original_relays = secret.Relays();
        CBigNum original_shared_secret;
        original_shared_secret = Hash(secret_ * original_relays[position]);
        if ((secret_ ^ original_shared_secret) ==
            secret.secrets_xor_shared_secrets[position])
            return false;
        return true;
    }

/*
 *  AddressPartSecretDisclosure
 ********************************/
