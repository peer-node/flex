#include <map>

#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

#include <stdint.h>
#include "crypto/key.h"
#include "crypto/polynomial.h"


void printPolynomial_(BigNumPolynomial poly)
{
    for (uint32_t i = 0; i < poly.size(); i++)
        printf("%s,", poly.coefficients[i].ToString().c_str());
}

std::vector<CBigNum> PositiveIntegers(uint32_t N) {
    std::vector<CBigNum> result;

    if (N == 0)
        return result;
    
    CBigNum one = 1;
    result.push_back(one);

    for (uint32_t i = 0; i < N - 1; i++) 
    {
        CBigNum next;
        next = result[result.size() - 1] + 1;
        result.push_back(next);
    }
    return result;
}


BigNumPolynomial::BigNumPolynomial() {}


BigNumPolynomial::BigNumPolynomial(uint8_t curve, std::vector<CBigNum> coeffs):
    curve(curve)
{
    coefficients = coeffs;
}


BigNumPolynomial::BigNumPolynomial(uint8_t curve, uint64_t k):
    curve(curve)
{
    for (uint32_t i = 0; i < k; i++) 
    {
        CBigNum coeff = 0;
        coefficients.push_back(coeff);
    }
}

void BigNumPolynomial::zero() 
{
    for (uint32_t i = 0; i < coefficients.size(); i++)
        coefficients[i] = 0;
}

void BigNumPolynomial::randomize() 
{
    for (uint32_t i = 0; i < coefficients.size(); i++)
        coefficients[i].Randomize(Point(curve).Modulus());
}

uint64_t BigNumPolynomial::size() 
{
    return coefficients.size();
}


CBigNum BigNumPolynomial::evaluate(CBigNum x) 
{
    CBigNum modulus = Point(curve).Modulus();
    CBigNum result;
    CBigNum power_of_x = 1;
    CBigNum power_of_x_;
    CBigNum term;

    result = 0;

    for(uint32_t i = 0; i < coefficients.size(); i++) 
    {
        term = (power_of_x * coefficients[i]) % modulus;
        result = (result + term) % modulus;
        power_of_x = (power_of_x * x) % modulus;
    }
    return result;
}

uint32_t BigNumPolynomial::multiply_power(uint32_t power) 
{
    for(uint32_t i = 0; i < power; i++) 
    {
        CBigNum coeff = 0;
        coefficients.insert(coefficients.begin(), coeff);
    }
    return 1;
}

uint32_t BigNumPolynomial::multiplyBigNum(CBigNum y)
{
    for(uint32_t i = 0; i < coefficients.size(); i++) 
    {
        coefficients[i] = (y * coefficients[i]) % Point(curve).Modulus();
    }
    return 1;
}

uint32_t BigNumPolynomial::addBigNumPolynomial(const BigNumPolynomial& poly) 
{
    CBigNum coeff_;
    for(uint32_t i = 0; i < poly.coefficients.size()
                        && i < coefficients.size(); i++) 
    {
        if (i == coefficients.size()) 
        {
            CBigNum coeff = 0;
            coefficients.push_back(coeff);
        }
        coeff_ = (coefficients[i] + poly.coefficients[i]) 
                 % Point(curve).Modulus();
        coefficients[i] = coeff_;
    }
    return 1;
}


int BNPoly_multiply(BigNumPolynomial &result,
                    BigNumPolynomial poly1,
                    BigNumPolynomial poly2) 
{
    BigNumPolynomial term;

    result.zero();
    for (uint32_t i = 0; i < poly2.size();i ++) 
    {
        term = BigNumPolynomial(poly1.curve, poly1.coefficients);
        term.multiplyBigNum(poly2.coefficients[i]);
        term.multiply_power(i);
        result.addBigNumPolynomial(term);
    }
    return 1;
}


void Poly_swap(BigNumPolynomial &poly1, BigNumPolynomial &poly2) 
{
    std::vector<CBigNum> temp = poly1.coefficients;
    poly1.coefficients = poly2.coefficients;
    poly2.coefficients = temp;
}

BigNumPolynomial LagrangePoly(uint8_t curve,
                              uint32_t j, 
                              std::vector<CBigNum> values)
{
    CBigNum modulus = Point(curve).Modulus();
    BigNumPolynomial result = BigNumPolynomial(curve, 1);
    result.coefficients[0] = 1;

    CBigNum difference;
    CBigNum inverse;
    CBigNum minus_one;

    minus_one = modulus - 1;

    BigNumPolynomial factor = BigNumPolynomial(curve, 2);
    BigNumPolynomial product = BigNumPolynomial(curve, 2);
 
    for (uint32_t k=0; k < values.size(); k++) 
    {
        if (j == k) continue;

        difference = (values[j] - values[k]) % modulus;
        inverse = difference.InverseMod(modulus);

        factor.zero();
        factor.coefficients[0] = (minus_one * values[k]) % modulus;
        factor.coefficients[1] = 1;
        factor.multiplyBigNum(inverse);
        BNPoly_multiply(product, result, factor);
        Poly_swap(result, product);
    }
    return result;
}

PointPolynomial::PointPolynomial(uint8_t curve, 
                                 std::vector<Point> point_coeffs):
    curve(curve)
{
    point_coefficients = point_coeffs;
}


Point PointPolynomial::evaluate(CBigNum x) 
{
    Point result(SECP256K1);
    CBigNum power_of_x = 1;
    Point term(SECP256K1);

    for(uint32_t i = 0; i < point_coefficients.size(); i++) {
        term = power_of_x * point_coefficients[i];
        result = result + term;
        power_of_x = (power_of_x * x) % Point(curve).Modulus();
    }

    return result;
}

