#include "../../test/teleport_tests/teleport_data/TestData.h"
#include <map>

#include <openssl/ecdsa.h>

#include "crypto/key.h"
#include "crypto/point.h"
#include "crypto/polynomial.h"
#include "crypto/teleportcrypto.h"
#include "relays/successionsecret.h"
#include "trade/tradesecret.h"
#include "deposit/deposit_secret.h"

#include "log.h"
#define LOG_CATEGORY "teleportcrypto.cpp"


using namespace std;


uint64_t RelaysRequired(uint64_t relays_in_total)
{
    if (relays_in_total == NUM_SUCCESSION_SECRETS)
        return SUCCESSION_SECRETS_NEEDED;
    else if (relays_in_total == SECRETS_PER_TRADE)
        return TRADE_SECRETS_NEEDED;
    else if (relays_in_total == SECRETS_PER_ADDRESS_PART)
        return ADDRESS_PART_SECRETS_NEEDED;

    log_ << "RelaysRequired: don't know how many required for "
         << relays_in_total << "\n";

    return 0;
}

CBigNum random_private(uint8_t curve)
{
    CBigNum privkey;
    privkey.Randomize(Point(curve).Modulus());
    return privkey;
}

vector<Point> publics_from_privates(uint8_t curve, vector<CBigNum> secrets)
{
    vector<Point> publics;

    for (uint64_t i = 0; i < secrets.size(); i++)
    {
        publics.push_back(Point(curve, secrets[i]));
    }
    return publics;
}



int generate_key_with_secret_parts(uint8_t curve,
                                   Point& pubkey,
                                   vector<CBigNum>& secrets,
                                   vector<CBigNum>& coeffs,
                                   uint32_t N,
                                   CBigNum *privkey) 
{
    log_ << "generate_key_with_secret_parts\n";
    log_ << "curve is " << curve << "\n";
    log_ << "generating polynomial of size " << RelaysRequired(N) << "\n";
    BigNumPolynomial poly = BigNumPolynomial(curve, RelaysRequired(N));
    log_ << "randomizing\n";
    poly.randomize();

    if (privkey != NULL && *privkey != 0)
    {
#ifndef _PRODUCTION_BUILD
        log_ << "using given privkey " << *privkey << "\n";
#endif
        poly.coefficients[0] = *privkey;
    }

    coeffs = poly.coefficients;

    vector<CBigNum> one_two_three_etc = PositiveIntegers(N);
    for (uint32_t i = 0; i < N; i++) 
    {
        CBigNum value = poly.evaluate(one_two_three_etc[i]);
        secrets.push_back(value);
    }
    pubkey = Point(curve, coeffs[0]);
    log_ << "pubkey is " << pubkey << "\n";

    *privkey = coeffs[0];
#ifndef _PRODUCTION_BUILD
    log_ << "coeffs[0] is " << coeffs[0] << "\n";
    log_ << "privkey is " << *privkey << "\n";
#endif
    return 1;
}


int generate_private_key_from_N_secret_parts(
        uint8_t curve,
        CBigNum& privkey,
        vector<CBigNum>& secrets,
        vector<Point>& commitments,
        uint32_t N,
        bool& cheated,
        vector<int32_t>& cheaters) 
{
    vector<CBigNum> verified_secrets;
    vector<CBigNum> x_values = PositiveIntegers(N);
    vector<CBigNum> x_inputs;

    for (uint32_t i = 0; i < N; i++) 
    {
        if (!secrets[i])
            continue;
        Point check(curve, secrets[i]);
        if (check != commitments[i])
        {
            cheaters.push_back(i);
        }
        else 
        {
            verified_secrets.push_back(secrets[i]);
            x_inputs.push_back(x_values[i]);
        }
    }

    uint32_t K = RelaysRequired(N);
    cheated = (cheaters.size() > 0);
    if (verified_secrets.size() < K)
        return -1;

    verified_secrets.resize(K);
    x_inputs.resize(K);

    Point pubkey = generate_private_key_from_K_secret_parts(curve, 
                                                            privkey, 
                                                            verified_secrets, 
                                                            x_inputs, 
                                                            RelaysRequired(N));

    return 1;
}


Point generate_private_key_from_K_secret_parts(
        uint8_t curve,
        CBigNum& privkey,
        vector<CBigNum>& secrets,
        vector<CBigNum>& x_values,
        uint32_t K) 
{
    if (secrets.size() < K)
        return -1;
    CBigNum modulus = Point(curve).Modulus();
    CBigNum result = 0;
    CBigNum zero = 0;
    CBigNum sum = 0;
    CBigNum term;
    CBigNum lagrangeCoeff;

    for (uint64_t i = 0; i < K; i++) 
    {
        BigNumPolynomial LPoly = LagrangePoly(curve, i, x_values);
        lagrangeCoeff = LPoly.evaluate(zero);
        term = (lagrangeCoeff * secrets[i]) % modulus;
        result = (term + result) % modulus;
    }
    privkey = result;

    return Point(curve, privkey);
}


Point generate_public_key_from_point_coefficients(
        vector<Point>& point_coefficients) 
{
    return point_coefficients[0];
}


bool confirm_public_key_with_N_polynomial_point_values(
        Point& pubkey,
        vector<Point> polynomial_point_values,
        uint32_t N) 
{
    uint8_t curve = polynomial_point_values[0].curve;

    if (polynomial_point_values.size() != N)
        return false;

    vector<CBigNum> x_values = PositiveIntegers(N);
    vector<Point> point_values;
    point_values = polynomial_point_values;
    Point pubkey_;

    uint32_t K = RelaysRequired(N);
    x_values.resize(K);
    point_values.resize(K);

    generate_public_key_from_K_polynomial_point_values(
           curve, pubkey_, point_values, x_values, K);

    return (pubkey_ == pubkey);
}


int generate_public_key_from_K_polynomial_point_values(
        uint8_t curve,
        Point& pubkey,
        vector<Point>& point_values,
        vector<CBigNum>& x_values,
        uint32_t K) 
{
    pubkey = Point(curve);
    pubkey.SetToInfinity();

    CBigNum zero = 0;
    Point term;
    CBigNum lagrangeCoeff;

    for (uint64_t i = 0; i < K; i++) 
    {
        BigNumPolynomial LPoly = LagrangePoly(curve, i, x_values);
        lagrangeCoeff = LPoly.evaluate(zero);
        term = lagrangeCoeff * point_values[i];
        pubkey = term + pubkey;
    }

    return 1;
}
