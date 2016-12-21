// Copyright (c) 2014 Teleport Developers
// Distributed under version 3 of the Gnu Affero GPL software license, see the accompanying
// file COPYING for details

#ifndef TELEPORT_SECP256K1POINT
#define TELEPORT_SECP256K1POINT

#include "boost/variant.hpp"
#include "crypto/uint256.h"
#include "crypto/bignum.h"
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include "base/serialize.h"
#include "define.h"
#include <stdint.h>

#include "log.h"
#define LOG_CATEGORY "secp256k1point.h"

/** Errors thrown by the Secp256k1Point class */
class point_error : public std::runtime_error
{
public:
    explicit point_error(const string_t& str) : std::runtime_error(str) {}
};

/** C++ wrapper for OpenSSL EC_GROUP on curve secp256k1 */
class CGroup
{
public:
    EC_GROUP *group;
    BN_CTX* context;
    BIGNUM *modulus;
    const EC_POINT *generator;

    CGroup()
    {
        group = EC_GROUP_new_by_curve_name(NID_secp256k1);
        context = BN_CTX_new();
        EC_GROUP_precompute_mult(group, context);
        modulus = BN_new();
        EC_GROUP_get_order(group, modulus, context);
        generator = EC_GROUP_get0_generator(group);
    }

    ~CGroup()
    {
        EC_GROUP_clear_free(group);
        if (modulus)
            BN_free(modulus);
        if (context != NULL)
            BN_CTX_free(context);
        CRYPTO_cleanup_all_ex_data();
    }
};

static CGroup curve_group;

/** C++ wrapper for OpenSSL EC_POINT class, for points on secp256k1 */
static uint32_t num_points{1};

class Secp256k1Point 
{
public:
    EC_POINT *point{NULL};
    uint32_t number;
    BN_CTX* context;
        
    Secp256k1Point()
    {
        context = BN_CTX_new();
        point = EC_POINT_new(curve_group.group);
        SetToInfinity();
        number = num_points++;
    }

    Secp256k1Point(const EC_POINT *ecp) 
    {
        context = BN_CTX_new();
        point = EC_POINT_new(curve_group.group);
        if (!EC_POINT_copy(point, ecp))
            throw point_error("Secp256k1Point(EC_POINT *) : EC_POINT_copy failed");
        number = num_points++;
    }

    Secp256k1Point(const CBigNum& bn) 
    {
        context = BN_CTX_new();
        point = EC_POINT_new(curve_group.group);
        SetToMultipleOfGenerator(bn);
        number = num_points++;
    }
    
    Secp256k1Point(const Secp256k1Point &p) 
    {
        context = BN_CTX_new();
        point = EC_POINT_new(curve_group.group);
        *this = p;
        number = num_points++;
    }

    Secp256k1Point(const vch_t vch) 
    {
        context = BN_CTX_new();
        point = EC_POINT_new(curve_group.group);
        setvch(vch);
        number = num_points++;
    }

    ~Secp256k1Point()
    {
        if (point != NULL)
            EC_POINT_clear_free(point);
        if (context != NULL)
            BN_CTX_free(context);
    }

    Secp256k1Point& operator=(const Secp256k1Point& p)
    {
        number = p.number;
        if (!EC_POINT_copy(point, p.point))
            throw point_error("Secp256k1Point::operator= : EC_POINT_copy failed");
        return *this;
    }

    Secp256k1Point& Set_EC_POINT(const EC_POINT *ecp)
    {
        if (!EC_POINT_copy(point, ecp))
            throw point_error("Secp256k1Point::Set_EC_POINT : EC_POINT_copy failed");
        return *this;
    }

    Secp256k1Point& operator+=(const Secp256k1Point& p)
    {
        EC_POINT_add(curve_group.group, point, point, p.point, context);
        return *this;
    }

    Secp256k1Point& operator-=(const Secp256k1Point& p)
    {
        EC_POINT_add(curve_group.group, point, point, (-p).point, context);
        return *this;
    }

    bool operator!()
    {
        return IsAtInfinity();
    }

    Secp256k1Point& SetToMultipleOfGenerator(const CBigNum& bn)
    {
        if (!EC_POINT_copy(point, curve_group.generator))
            throw point_error("Secp256k1Point::SetToMultipleOfGenerator:copy failed");
        EC_POINT_mul(curve_group.group, point, 
                     &bn, point, curve_group.modulus, context);
        return *this;
    }

    static CBigNum Modulus()
    {
        return CBigNum(curve_group.modulus);
    }

    void SetToInfinity()
    {
        EC_POINT_set_to_infinity(curve_group.group, point);
    }

    bool IsAtInfinity()
    {
        EC_POINT *inf = EC_POINT_new(curve_group.group);
        EC_POINT_set_to_infinity(curve_group.group, inf);
        bool result = !EC_POINT_cmp(curve_group.group, point, inf, context);
        EC_POINT_free(inf);
        return result;
    }

    void GetCoordinates(CBigNum &x, CBigNum &y) const
    {
        EC_POINT_get_affine_coordinates_GFp(curve_group.group, point,
                                            &x, &y, context);
    }

    std::string ToString() const
    {
        CBigNum x, y;
        GetCoordinates(x, y);
        return x.ToString() + "," + y.ToString();
    }

    Secp256k1Point operator-() const
    {
        Secp256k1Point result;
        result = *this;
        EC_POINT_invert(curve_group.group, result.point, context);
        return result;
    }

    bool operator==(const Secp256k1Point& p) const
    {
        return !EC_POINT_cmp(curve_group.group, point, p.point, context);
    }

    bool operator!=(const Secp256k1Point& p)
    {
        return (bool)EC_POINT_cmp(curve_group.group, point, p.point, context);
    }

    bool operator<(const Secp256k1Point& p) const
    {
        CBigNum x, y, x_, y_;
        GetCoordinates(x, y);
        p.GetCoordinates(x_, y_);
        return std::make_pair(x, y) < std::make_pair(x_, y_);
    }

    Secp256k1Point& operator*=(const CBigNum& bn);

    vch_t getvch() const
    {
        vch_t vchPoint;
        vchPoint.resize(33);
        EC_POINT_point2oct(curve_group.group, point, 
                           POINT_CONVERSION_COMPRESSED,
                           &vchPoint[0], 33, context);
        return vchPoint;
    }

    bool setvch(const vch_t& vchPoint)
    {
        bool empty = true;
        for (uint32_t i = 0; i < vchPoint.size(); i++)
            if (vchPoint[i] != 0)
            {
                empty = false;
                break;
            }
        if (empty)
            return false;

        if (point != NULL)
        {
            EC_POINT_free(point);
            point = EC_POINT_new(curve_group.group);
        }

        bool result = (EC_POINT_oct2point(curve_group.group, point, &vchPoint[0], 33, context) != 0);
        return result;
    }

    operator vch_t()
    {
        return getvch();
    }

    unsigned int GetSerializeSize(int nType=0, int nVersion=PROTOCOL_VERSION) 
    const
    {
        return ::GetSerializeSize(getvch(), nType, nVersion);
    }

    template<typename Stream>
    void Serialize(Stream& s, int nType=0, int nVersion=PROTOCOL_VERSION) 
    const
    {
        ::Serialize(s, getvch(), nType, nVersion);
    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType=0, int nVersion=PROTOCOL_VERSION)
    {
        std::vector<unsigned char> vch;
        ::Unserialize(s, vch, nType, nVersion);
        setvch(vch);
    }
};


inline Secp256k1Point operator+(const Secp256k1Point& a, const Secp256k1Point& b)
{
    Secp256k1Point r;
    BN_CTX* context;
    context = BN_CTX_new();
    if (!EC_POINT_add(curve_group.group, r.point, a.point, b.point, context))
        throw point_error("operator+(Cpoint,Secp256k1Point)) : EC_POINT_add failed");
    BN_CTX_free(context);
    return r;
}


inline Secp256k1Point operator-(const Secp256k1Point& a, const Secp256k1Point& b)
{
    Secp256k1Point r;
    Secp256k1Point d = -b;
    BN_CTX* context;
    context = BN_CTX_new();
    if (!EC_POINT_add(curve_group.group, r.point, a.point, d.point, context))
        throw point_error("operator-(Cpoint,Secp256k1Point)) : EC_POINT_add failed");
    BN_CTX_free(context);
    return r;
}


inline Secp256k1Point operator*(const Secp256k1Point& p, const CBigNum& bn)
{
    Secp256k1Point r;
    BN_CTX* context;
    context = BN_CTX_new();
    if (!EC_POINT_mul(curve_group.group, r.point, 
                      NULL, p.point, &bn, context))
        throw point_error("operator*(Cpoint,CBigNum)) : EC_POINT_add failed");
    BN_CTX_free(context);
    return r;
}


inline Secp256k1Point operator*(const CBigNum& bn, const Secp256k1Point& p)
{
    Secp256k1Point r;
    BN_CTX* context;
    context = BN_CTX_new();
    if (!EC_POINT_mul(curve_group.group, r.point, 
                      NULL, p.point, &bn, context))
        throw point_error("operator*(CBigNum,Secp256k1Point)) : EC_POINT_mul failed");
    BN_CTX_free(context);
    return r;
}


inline Secp256k1Point& Secp256k1Point::operator*=(const CBigNum& bn)
{
    *this = bn * (*this);
    return *this;
}

#endif
