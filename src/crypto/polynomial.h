// Copyright (c) 2014 Flex Developers
// Distributed under version 3 of the Gnu Affero GPL software license, see the accompanying
// file COPYING for details

#ifndef FLEX_POLYNOMIAL_H
#define FLEX_POLYNOMIAL_H

#include <map>

#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

#include "crypto/point.h"
#include "crypto/key.h"

std::vector<CBigNum> PositiveIntegers(unsigned int N);


class BigNumPolynomial 
{
public:
    uint8_t curve;
    std::vector<CBigNum> coefficients;

    BigNumPolynomial(uint8_t curve, std::vector<CBigNum> coeffs);
    BigNumPolynomial();
    BigNumPolynomial(uint8_t curve, uint64_t k);

    void zero();

    void randomize();

    uint64_t size();

    CBigNum evaluate(CBigNum x);

    uint32_t multiply_power(uint32_t power);

    uint32_t multiplyBigNum(CBigNum y);

    uint32_t addBigNumPolynomial(const BigNumPolynomial& poly);
};


int BNPoly_multiply(BigNumPolynomial &result,
                    BigNumPolynomial poly1,
                    BigNumPolynomial poly2);


void Poly_swap(BigNumPolynomial &poly1, BigNumPolynomial &poly2);


BigNumPolynomial LagrangePoly(uint8_t curve, 
                              uint32_t j, 
                              std::vector<CBigNum> x_values);


class PointPolynomial 
{
public:
    uint8_t curve;
    std::vector<Point> point_coefficients;

    PointPolynomial(uint8_t curve, std::vector<Point> point_coeffs);

    Point evaluate(CBigNum x);
};

#endif
