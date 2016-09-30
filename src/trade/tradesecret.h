#ifndef TELEPORT_TRADE_SECRET
#define TELEPORT_TRADE_SECRET

#include "crypto/polynomial.h"
#include "crypto/teleportcrypto.h"
#include "relays/relaystate.h"

#ifndef _PRODUCTION_BUILD

#define SECRETS_PER_TRADE 5
#define TRADE_SECRETS_NEEDED 4

#else

#define SECRETS_PER_TRADE 34
#define TRADE_SECRETS_NEEDED 31

#endif

#include "log.h"
#define LOG_CATEGORY "tradesecret.h"

class DistributedTradeSecretDisclosure;

class DistributedTradeSecret
{
public:
    uint160 credit_hash;
    uint160 relay_chooser;
    uint160 relay_list_hash;
    std::vector<Point> credit_coefficient_points;
    std::vector<Point> currency_coefficient_points;
    std::vector<PieceOfSecret> pieces;

    std::vector<Point> relays;
    CBigNum credit_privkey;
    CBigNum currency_privkey;

    std::vector<CBigNum> credit_coefficients;
    std::vector<CBigNum> currency_coefficients;

    DistributedTradeSecret() { }
    ~DistributedTradeSecret() { }

    DistributedTradeSecret(uint8_t curve,
                           uint160 credit_hash,
                           uint160 relay_chooser):
        credit_hash(credit_hash),
        relay_chooser(relay_chooser)
    {
        relays = Relays();
        relay_list_hash = PointListHash(relays);
        std::vector<CBigNum> credit_secrets, currency_secrets;
        Point credit_pubkey, currency_pubkey;

        generate_key_with_secret_parts(SECP256K1,
                                       credit_pubkey,
                                       credit_secrets,
                                       credit_coefficients,
                                       relays.size(),
                                       &credit_privkey);
#ifndef _PRODUCTION_BUILD
        log_ << "DistributedTradeSecret(): credit privkey is "
             << credit_privkey << " and credit pubkey is " 
             << credit_pubkey << "\n";
#endif

        credit_coefficient_points = publics_from_privates(SECP256K1,
                                                          credit_coefficients);
        generate_key_with_secret_parts(curve,
                                       currency_pubkey,
                                       currency_secrets,
                                       currency_coefficients,
                                       relays.size(),
                                       &currency_privkey);
        currency_coefficient_points
            = publics_from_privates(SECP256K1, currency_coefficients);

        keydata[credit_pubkey]["privkey"] = credit_privkey;
#ifndef _PRODUCTION_BUILD
        log_ << "recorded " << credit_privkey << " as privkey of "
             << credit_pubkey << "\n";
#endif

        keydata[currency_pubkey]["privkey"] = currency_privkey;

        for (uint32_t i = 0; i < relays.size(); i++)
        {
            PieceOfSecret piece(credit_secrets[i],
                                currency_secrets[i],
                                relays[i],
                                curve);
            keydata[Point(SECP256K1, credit_secrets[i])]["privkey"]
                = credit_secrets[i];
            log_ << "DistributedTradeSecret(): Storing: secret "
                 << credit_secrets[i] << " with pubkey " 
                 << Point(SECP256K1, credit_secrets[i]) << "\n";
            keydata[Point(curve, currency_secrets[i])]["privkey"]
                = currency_secrets[i];
            pieces.push_back(piece);
            log_ << "DistributedTradeSecret: pieces " << i 
                 << " is " << piece << "\n";
        }
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_hash);
        READWRITE(relay_chooser);
        READWRITE(relay_list_hash);
        READWRITE(credit_coefficient_points);
        READWRITE(currency_coefficient_points);
        READWRITE(pieces);
    )

    string_t ToString() const
    {
        std::stringstream ss;

        ss << "\n============= DistributedTradeSecret ===========\n"
           << "== Credit Hash: " << credit_hash.ToString() << "\n"
           << "== Relay Chooser: " << relay_chooser.ToString() << "\n"
           << "== Relay List Hash: " << relay_list_hash.ToString() << "\n"
           << "== credit_coefficient_points: \n";
        foreach_(Point point, credit_coefficient_points)
            ss << "== " << point.ToString() << "\n";
        ss << "==\n"
           << "== currency_coefficient_points: \n";
        foreach_(Point point, currency_coefficient_points)
            ss << "== " << point.ToString() << "\n";
        ss << "==\n"
           << "== Pieces: " << pieces.size() << "\n"
           << "=========== End DistributedTradeSecret =========\n";
        return ss.str(); 
    }

    std::vector<Point> Relays()
    {
        if (relays.size() == 0)
        {
            RelayState state = GetRelayState(credit_hash);
            relays = state.ChooseRelays(relay_chooser, SECRETS_PER_TRADE);
        }
        return relays;
    }

    std::vector<Point> CreditPointValues()
    {
        std::vector<Point> point_values;
        foreach_(PieceOfSecret piece, pieces)
            point_values.push_back(piece.credit_pubkey);
        return point_values;
    }

    std::vector<Point> CurrencyPointValues()
    {
        std::vector<Point> point_values;
        foreach_(PieceOfSecret piece, pieces)
            point_values.push_back(piece.currency_pubkey);
        return point_values;
    }

    bool Validate()
    {
        if (credit_coefficient_points.size() != TRADE_SECRETS_NEEDED   ||
            currency_coefficient_points.size() != TRADE_SECRETS_NEEDED ||
            pieces.size() != SECRETS_PER_TRADE)
            return false;

        relays = Relays();

        log_ << "DistributedTradeSecret::Validate():\n"
             << "relays = " << relays
             << "computed relay list hash: " << PointListHash(relays)
             << "reported relay list hash: " << relay_list_hash << "\n";

        log_ << "DistributedTradeSecret::Validate():\n"
             << "ValidateKeys(): " 
             << ValidateKeys() << "\n";

        return PointListHash(relays) == relay_list_hash && ValidateKeys();
    }

    bool ValidateKeys()
    {
        uint32_t n = SECRETS_PER_TRADE;
        std::vector<CBigNum> one_two_three_etc = PositiveIntegers(n);
        std::vector<Point> credit_points, currency_points;
        PointPolynomial ppoly(SECP256K1, credit_coefficient_points);

        for (uint32_t i = 0; i < n; i++) 
        {
            Point evaluation = ppoly.evaluate(one_two_three_etc[i]);
            if (evaluation != pieces[i].credit_pubkey)
            {
                log_ << "ValidateKeys: credit polynomial evaluation failed:\n"
                     << evaluation << " vs " << pieces[i].credit_pubkey
                     << "\n";
                return false;
            }
            credit_points.push_back(evaluation);
        }

        ppoly = PointPolynomial(currency_coefficient_points[0].curve,
                                currency_coefficient_points);

        for (uint32_t i = 0; i < n; i++) 
        {
            Point evaluation = ppoly.evaluate(one_two_three_etc[i]);
            if (evaluation != pieces[i].currency_pubkey)
            {
                log_ << "ValidateKeys: currency polynomial evaluation failed:\n"
                     << evaluation << " vs " << pieces[i].credit_pubkey
                     << "\n";
                return false;
            }
            currency_points.push_back(evaluation);
        }

        Point credit_pubkey = CreditPubKey();
        Point currency_pubkey = CurrencyPubKey();

        return confirm_public_key_with_N_polynomial_point_values(
                    credit_pubkey, credit_points, n) &&
               confirm_public_key_with_N_polynomial_point_values(
                    currency_pubkey, currency_points, n);
    }

    bool ValidateAndStoreMySecretPieces(
        BigNumsInPositions& bad_shared_secrets,
        BigNumsInPositions& credit_secrets_that_dont_recover_currency_secrets)
    {
        BigNumsInPositions currency_failures;
        relays = Relays();
        bool ok = true;
        for (uint32_t i = 0; i < relays.size(); i++)
        {
            if (!keydata[relays[i]].HasProperty("privkey"))
                continue;
            PieceOfSecret piece = pieces[i];
            CBigNum privkey = keydata[relays[i]]["privkey"];
            CBigNum credit_secret, currency_secret;

            if (piece.RecoverSecrets(credit_secret, 
                                     currency_secret, privkey))
            {
                keydata[piece.credit_pubkey]["privkey"] = credit_secret;
                keydata[piece.currency_pubkey]["privkey"]
                    = currency_secret;
                continue;
            }
            
            ok = false;
            if (Point(SECP256K1, credit_secret) != piece.credit_pubkey)
            {
                bad_shared_secrets.push_back(
                    std::make_pair(i, Hash(privkey * piece.credit_pubkey)));
                continue;
            }

            CBigNum recovered_currency_secret
                = Hash(Point(SECP256K1, credit_secret)) ^  
                  piece.currency_secret_xor_hash_of_credit_secret;
            if (Point(piece.currency_pubkey.curve,
                      recovered_currency_secret)
                != piece.currency_pubkey)
            {
                currency_failures.push_back(std::make_pair(i, credit_secret));
            }
        }
        credit_secrets_that_dont_recover_currency_secrets = currency_failures;
        return ok;
    }

    Point CreditPubKey()
    {
        return credit_coefficient_points[0];
    }

    CBigNum CreditPrivKey()
    {
        return credit_privkey;
    }

    CBigNum RecoverCreditPrivKey()
    {
        CBigNum privkey;
        std::vector<CBigNum> secrets;
        bool cheated;
        std::vector<int32_t> cheaters;
        log_ << "RecoverCreditPrivKey()\n";
        std::vector<Point> commitments;
        foreach_(const PieceOfSecret& piece, pieces)
        {
            CBigNum secret = keydata[piece.credit_pubkey]["privkey"];
#ifndef _PRODUCTION_BUILD
            log_ << "adding secret " << secret << "\n";
#endif
            secrets.push_back(secret);
            commitments.push_back(piece.credit_pubkey);
        }

        generate_private_key_from_N_secret_parts(SECP256K1,
                                                 credit_privkey,
                                                 secrets,
                                                 commitments,
                                                 secrets.size(),
                                                 cheated,
                                                 cheaters);
        return credit_privkey;
    }

    CBigNum RecoverCurrencyPrivKey()
    {
        std::vector<CBigNum> secrets;
        bool cheated;
        std::vector<int32_t> cheaters;
        log_ << "RecoverCreditPrivKey()\n";
        std::vector<Point> commitments;
        foreach_(const PieceOfSecret& piece, pieces)
        {
            CBigNum secret = keydata[piece.currency_pubkey]["privkey"];
#ifndef _PRODUCTION_BUILD
            log_ << "adding secret " << secret << "\n";
#endif
            secrets.push_back(secret);
            commitments.push_back(piece.currency_pubkey);
        }
        uint8_t curve = pieces[0].currency_pubkey.curve;
        generate_private_key_from_N_secret_parts(curve,
                                                 currency_privkey,
                                                 secrets,
                                                 commitments,
                                                 secrets.size(),
                                                 cheated,
                                                 cheaters);
        return currency_privkey;
    }

    Point CurrencyPubKey()
    {
        return currency_coefficient_points[0];
    }
    
    CBigNum CurrencyPrivKey()
    {
        return currency_privkey;
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


class DistributedTradeSecretDisclosure
{
public:
    uint160 credit_hash;
    uint160 relay_chooser;
    uint160 relay_list_hash;
    std::vector<RevelationOfPieceOfSecret> revelations;

    std::vector<Point> relays;

    DistributedTradeSecretDisclosure() { }
    ~DistributedTradeSecretDisclosure() { }

    DistributedTradeSecretDisclosure(DistributedTradeSecret& secret,
                                     uint160 relay_chooser):
        credit_hash(secret.credit_hash),
        relay_chooser(relay_chooser)
    {
        std::vector<Point> relays = Relays();
        relay_list_hash = PointListHash(relays);

        for (uint32_t i = 0; i < secret.pieces.size(); i++)
        {
            Point credit_pubkey = secret.pieces[i].credit_pubkey;
#ifndef _PRODUCTION_BUILD
            log_ << "DistributedTradeSecretDisclosure: secret piece " << i
                 << " is " << secret.pieces[i] << "\n";
#endif
            CBigNum credit_secret = keydata[credit_pubkey]["privkey"];
            log_ << "DistributedTradeSecretDisclosure(): " << i
                 << " credit pubkey is " << credit_pubkey
#ifndef _PRODUCTION_BUILD
                 << " credit_secret is " << credit_secret 
#endif
                 << "\n";
            RevelationOfPieceOfSecret revelation(relays[i], credit_secret);
            revelations.push_back(revelation);
        }
    }

    std::vector<Point> Relays()
    {
        if (relays.size() == 0)
        {
            RelayState state = GetRelayState(credit_hash);
            relays = state.ChooseRelays(relay_chooser, SECRETS_PER_TRADE);
        }
        return relays;
    }

    DistributedTradeSecretDisclosure(uint160 credit_hash,
                                     uint160 relay_chooser,
                                     DistributedTradeSecret& secret):
        credit_hash(credit_hash),
        relay_chooser(relay_chooser)
    {
        std::vector<Point> relays = Relays();
        relay_list_hash = PointListHash(relays);

        for (uint32_t i = 0; i < secret.pieces.size(); i++)
        {
            Point credit_pubkey = secret.pieces[i].credit_pubkey;
            CBigNum credit_secret = keydata[credit_pubkey]["privkey"];
            RevelationOfPieceOfSecret revelation(relays[i], credit_secret);
            revelations.push_back(revelation);
        }

    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_hash);
        READWRITE(relay_chooser);
        READWRITE(relay_list_hash);
        READWRITE(revelations);
    )

    bool Validate()
    {
        if (PointListHash(Relays()) != relay_list_hash)
        {
            log_ << "DistributedTradeSecretDisclosure::Validate(): "
                 << "bad relays: " << PointListHash(Relays())
                 << " != " << relay_list_hash << "\n";
            return false;
        }
        if (revelations.size() != SECRETS_PER_TRADE)
        {
            log_ << "DistributedTradeSecretDisclosure::Validate(): "
                 << "wrong number of revelations: "
                 << revelations.size() << " vs " << SECRETS_PER_TRADE << "\n";
            return false;
        }
        return true;
    }

    bool ValidateAndStoreMyRevelations(
        DistributedTradeSecret& secret,
        BigNumsInPositions& bad_revelation_shared_secrets,
        BigNumsInPositions& bad_currency_secrets,
        BigNumsInPositions& bad_original_shared_secrets)
    {
        relays = Relays();
        bool ok = true;
        for (uint32_t i = 0; i < relays.size(); i++)
        {
            if (!keydata[relays[i]].HasProperty("privkey"))
                continue;
            
            CBigNum privkey = keydata[relays[i]]["privkey"];
            CBigNum credit_secret, currency_secret;
            PieceOfSecret piece = secret.pieces[i];
            if (revelations[i].RecoverSecrets(credit_secret,
                                              currency_secret,
                                              privkey,
                                              piece))
            {
                keydata[piece.credit_pubkey]["privkey"] = credit_secret;
                keydata[piece.currency_pubkey]["privkey"] = currency_secret;
                keydata[piece.credit_pubkey]["known_via_disclosure"] = true;
                keydata[piece.currency_pubkey]["known_via_disclosure"] = true;
                continue;
            }

            ok = false;
            AddBadSecret(secret,
                         bad_revelation_shared_secrets,
                         bad_currency_secrets,
                         bad_original_shared_secrets,
                         i,
                         privkey);
        }
        return ok;
    }

    void AddBadSecret(DistributedTradeSecret& secret,
                      BigNumsInPositions& bad_revelation_shared_secrets,
                      BigNumsInPositions& bad_currency_secrets,
                      BigNumsInPositions& bad_original_shared_secrets,
                      uint32_t i,
                      CBigNum privkey)
    {
        PieceOfSecret piece = secret.pieces[i];
        CBigNum second_shared_credit_secret
            = Hash(privkey * piece.credit_pubkey);

        CBigNum credit_secret_xor_second_shared_secret
            = revelations[i].credit_secret_xor_second_shared_secret;

        if (Point(SECP256K1, 
                  second_shared_credit_secret ^ 
                  credit_secret_xor_second_shared_secret)
            != piece.credit_pubkey)
        {
            bad_revelation_shared_secrets.push_back(
                std::make_pair(i, second_shared_credit_secret));
            return;
        }

        CBigNum currency_secret_xor_hash
            = piece.currency_secret_xor_hash_of_credit_secret;

        if (Point(SECP256K1, 
                  Hash(Point(SECP256K1, second_shared_credit_secret))
                  ^ currency_secret_xor_hash)
            != piece.currency_pubkey)
        {
            CBigNum credit_secret = second_shared_credit_secret
                                    ^ credit_secret_xor_second_shared_secret;
            bad_currency_secrets.push_back(std::make_pair(i, credit_secret));
        }

        CBigNum credit_secret = second_shared_credit_secret ^ 
                                  credit_secret_xor_second_shared_secret;

        CBigNum original_shared_secret 
            = Hash(credit_secret * secret.Relays()[i]);

        if ((original_shared_secret ^ piece.credit_secret_xor_shared_secret)
            != credit_secret)
        {
            bad_original_shared_secrets.push_back(
                                             std::make_pair(i, credit_secret));
        }
    }
};



#endif