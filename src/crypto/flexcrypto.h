// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef FLEX_FLEXCRYPTO_H
#define FLEX_FLEXCRYPTO_H

#include <map>

#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

#include "crypto/point.h"


typedef std::vector<std::pair<uint32_t, CBigNum> > BigNumsInPositions;


uint64_t RelaysRequired(uint64_t relays_in_total);



std::vector<Point> publics_from_privates(uint8_t curve, 
                                         std::vector<CBigNum> secrets);



int generate_key_with_secret_parts(uint8_t curve,
                                   Point& pubkey,
                                   std::vector<CBigNum>& secrets,
                                   std::vector<CBigNum>& coeffs,
                                   uint32_t N,
                                   CBigNum *privkey = NULL);


int generate_private_key_from_N_secret_parts(uint8_t curve,
        CBigNum& privkey,
        std::vector<CBigNum>& secrets,
        std::vector<Point>& commitments,
        uint32_t N,
        bool& cheated,
        std::vector<int32_t>& cheaters);


Point generate_private_key_from_K_secret_parts(
        uint8_t curve,
        CBigNum& privkey,
        std::vector<CBigNum>& secrets,
        std::vector<CBigNum>& x_values,
        uint32_t K);


int generate_public_key_from_point_coefficients(
        Point& pubkey,
        std::vector<Point>& coefficients);


bool confirm_public_key_with_N_polynomial_point_values(
        Point& pubkey,
        std::vector<Point> point_values,
        uint32_t N);


int generate_public_key_from_K_polynomial_point_values(
        uint8_t curve,
        Point& pubkey,
        std::vector<Point>& point_values,
        std::vector<CBigNum>& x_values,
        uint32_t K);

inline CBigNum Hash(Point point)
{
    vch_t bytes = point.getvch();
    return CBigNum(Hash(bytes.begin(), bytes.end())) % point.Modulus();
}

inline CBigNum Hash(CBigNum number)
{
    vch_t bytes = number.getvch();
    return CBigNum(Hash(bytes.begin(), bytes.end()));
}

class RevelationOfPieceOfSecret;

class PieceOfSecret
{
public:
    Point credit_pubkey;
    Point currency_pubkey;
    
    CBigNum credit_secret_xor_shared_secret;
    CBigNum currency_secret_xor_hash_of_credit_secret;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_pubkey);
        READWRITE(currency_pubkey);
        READWRITE(credit_secret_xor_shared_secret);
        READWRITE(currency_secret_xor_hash_of_credit_secret);
    )

    string_t ToString()
    {
        std::stringstream ss;

        ss << "\n============= PieceOfSecret ===========\n"
           << "== Credit Pubkey: " << credit_pubkey.ToString() << "\n"
           << "== Currency Pubkey: " << currency_pubkey.ToString() << "\n"
           << "== \n"
           << "== Credit Secret ^ Shared Secret: "
           << credit_secret_xor_shared_secret.ToString() << "\n"
           << "== Currency Secret ^ Hash of Credit Secret: "
           << currency_secret_xor_hash_of_credit_secret.ToString() << "\n"
           << "=========== End PieceOfSecret =========\n";
        return ss.str();
    }

    PieceOfSecret() { }

    PieceOfSecret(const CBigNum& credit_secret, 
                  const CBigNum& currency_secret,
                  const Point recipient,
                  const uint8_t currency_curve):
        credit_pubkey(SECP256K1, credit_secret),
        currency_pubkey(currency_curve, currency_secret)
    {
        CBigNum shared_secret = Hash(credit_secret * recipient);
        credit_secret_xor_shared_secret
            = credit_secret ^ shared_secret;
        currency_secret_xor_hash_of_credit_secret
            = currency_secret ^ Hash(credit_secret);
    }

    bool RecoverSecrets(CBigNum& credit_secret, 
                        CBigNum& currency_secret, 
                        const CBigNum recipient_privkey)
    {
        CBigNum shared_secret = Hash(credit_pubkey * recipient_privkey);
        credit_secret = credit_secret_xor_shared_secret ^ shared_secret;
        currency_secret = currency_secret_xor_hash_of_credit_secret
                            ^ Hash(credit_secret);
        return (credit_pubkey == Point(credit_pubkey.curve, credit_secret) &&
                currency_pubkey == Point(currency_pubkey.curve, 
                                         currency_secret));
    }
};


class RevelationOfPieceOfSecret
{
public:
    CBigNum credit_secret_xor_second_shared_secret;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(credit_secret_xor_second_shared_secret);
    )

    string_t ToString()
    {
        std::stringstream ss;

        ss << "\n============= RevelationOfPieceOfSecret ===========\n"
           << "== \n"
           << "== Credit Secret ^ Second Shared Secret: "
           << credit_secret_xor_second_shared_secret.ToString() << "\n"
           << "== \n"
           << "=========== End RevelationOfPieceOfSecret =========\n";
        return ss.str();
    }

    RevelationOfPieceOfSecret() { }

    RevelationOfPieceOfSecret(Point recipient,
                              CBigNum credit_secret)
    {
        CBigNum second_shared_secret = Hash(credit_secret * recipient);
        credit_secret_xor_second_shared_secret = 
            credit_secret  ^ second_shared_secret;
    }

    bool RecoverSecrets(CBigNum& credit_secret, 
                        CBigNum& currency_secret, 
                        CBigNum recipient_privkey,
                        PieceOfSecret& piece)
    {
        CBigNum second_shared_secret = Hash(piece.credit_pubkey 
                                            * recipient_privkey);
        credit_secret = credit_secret_xor_second_shared_secret
                        ^ second_shared_secret;
        currency_secret = piece.currency_secret_xor_hash_of_credit_secret
                          ^ Hash(credit_secret);
        return piece.credit_pubkey == Point(piece.credit_pubkey.curve, 
                                            credit_secret)  &&
               piece.currency_pubkey == Point(piece.currency_pubkey.curve,
                                              currency_secret);
    }
};

#endif
