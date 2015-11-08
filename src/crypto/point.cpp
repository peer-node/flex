#include "crypto/25519points.h"
#include "crypto/secp256k1point.h"
#include "crypto/point.h"

#define FIAT 0
#define SECP256K1 1
#define CURVE25519 2
#define ED25519 3



/***********
 *  Diurn
 */

    Point::Point():
        curve(0),
        s_point(NULL),
        c_point(NULL),
        e_point(NULL)
    { }

    Point::Point(uint8_t curve):
        curve(curve),
        s_point(NULL),
        c_point(NULL),
        e_point(NULL)
    {
        if (curve == SECP256K1)
            s_point = new Secp256k1Point();
        else if (curve == CURVE25519)
            c_point = new Curve25519Point();
        else if (curve == ED25519)
            e_point = new Ed25519Point();
    }

    Point::Point(uint8_t curve, CBigNum n):
        curve(curve),
        s_point(NULL),
        c_point(NULL),
        e_point(NULL)
    {
        if (curve == SECP256K1)
            s_point = new Secp256k1Point();
        else if (curve == CURVE25519)
            c_point = new Curve25519Point();
        else if (curve == ED25519)
            e_point = new Ed25519Point();

        SetToMultipleOfGenerator(n);
    }

    Point::Point(const Point& other):
        curve(other.curve),
        s_point(NULL),
        c_point(NULL),
        e_point(NULL)
    {
        curve = other.curve;
        if (curve == SECP256K1 && other.s_point != NULL)
        {
            s_point = new Secp256k1Point();
            *s_point = *other.s_point;
        }
        else if (curve == CURVE25519 && other.c_point != NULL)
        {
            c_point = new Curve25519Point();
            *c_point = *other.c_point;
        }
        else if (curve == ED25519 && other.e_point != NULL)
        {
            e_point = new Ed25519Point();
            *e_point = *other.e_point;
        }
    }

    Point& Point::operator=(const Point& other)
    {
        curve = other.curve;
        if (curve == SECP256K1 && other.s_point != NULL)
        {
            if (!s_point)
                s_point = new Secp256k1Point();
            *s_point = *other.s_point;
        }
        else if (curve == CURVE25519 && other.c_point != NULL)
        {
            if (!c_point)
                c_point = new Curve25519Point();
            *c_point = *other.c_point;
        }
        else if (curve == ED25519 && other.e_point != NULL)
        {
            if (!e_point)
                e_point = new Ed25519Point();
            *e_point = *other.e_point;
        }
        return (*this);
    }

    Point::~Point()
    {
        if (s_point != NULL)
            delete s_point;
        if (c_point != NULL)
            delete c_point;
        if (e_point != NULL)
            delete e_point;
        
    }

    string_t Point::ToString() const
    {
        if (curve == SECP256K1)
            return (*s_point).ToString();
        else if (curve == CURVE25519)
            return (*c_point).ToString();
        else if (curve == ED25519)
            return (*e_point).ToString();
        return string_t();
    }

    void Point::SetToMultipleOfGenerator(CBigNum n)
    {
        if (curve == SECP256K1)
            (*s_point).SetToMultipleOfGenerator(n);
        else if (curve == CURVE25519)
            (*c_point).SetToMultipleOfGenerator(n);
        else if (curve == ED25519)
            (*e_point).SetToMultipleOfGenerator(n);
    }

    void Point::GetCoordinates(CBigNum &x, CBigNum &y) const
    {
       if (curve == SECP256K1)
            (*s_point).GetCoordinates(x, y);
        else if (curve == CURVE25519)
            (*c_point).GetCoordinates(x, y);
        else if (curve == ED25519)
            (*e_point).GetCoordinates(x, y);
    }

    void Point::SetToInfinity()
    {
        if (curve == SECP256K1)
            (*s_point).SetToInfinity();
        else if (curve == CURVE25519)
            (*c_point).SetToInfinity();
        else if (curve == ED25519)
            (*e_point).SetToInfinity();
    }

    bool Point::IsAtInfinity() const
    {
        return (*this) == Point(curve, 0);
    }

    bool Point::operator!() const
    {
        return IsAtInfinity();
    }

    bool Point::operator<(const Point& other) const
    {
        CBigNum x1, y1, x2, y2;
        GetCoordinates(x1, y1);
        other.GetCoordinates(x2, y2);
        return std::make_pair(x1, y1) < std::make_pair(x2, y2);
    }

    Point::operator vch_t()
    {
        if (curve == SECP256K1)
            return (*s_point).getvch();
        if (curve == CURVE25519)
            return (*c_point).getvch();
        if (curve == ED25519)
            return (*e_point).getvch();
        return vch_t();
    }

    bool Point::operator==(const Point& other) const
    {
        if (curve == SECP256K1)
            return *s_point == *other.s_point;
        else if (curve == CURVE25519)
            return *c_point == *other.c_point;
        else if (curve == ED25519)
            return *e_point == *other.e_point;
        return false;
    }

    bool Point::operator!=(const Point& other) const
    {
        return !(*this == other);
    }

    Point& Point::operator+=(const Point& other)
    {
        if (curve == SECP256K1)
            *s_point += *other.s_point;
        else if (curve == CURVE25519)
            *c_point += *other.c_point;
        else if (curve == ED25519)
            *e_point += *other.e_point;
        return (*this);
    }

    Point& Point::operator-=(const Point& other)
    {
        if (curve == SECP256K1)
            *s_point -= *other.s_point;
        else if (curve == CURVE25519)
            *c_point -= *other.c_point;
        else if (curve == ED25519)
            *e_point -= *other.e_point;
        return (*this);
    }

    Point& Point::operator*=(const CBigNum &n)
    {
        if (curve == SECP256K1)
            *s_point *= n;
        else if (curve == CURVE25519)
            *c_point *= n;
        else if (curve == ED25519)
            *e_point *= n;
        return (*this);
    }

    CBigNum Point::Modulus() const
    {
        if (curve == SECP256K1)
            return Secp256k1Point::Modulus();
        else if (curve == CURVE25519)
            return Curve25519Point::Modulus();
        else if (curve == ED25519)
            return Ed25519Point::Modulus();
        return CBigNum(0);
    }

    vch_t Point::getvch() const
    {
        vch_t point_bytes, bytes;
        if (curve == SECP256K1)
            point_bytes = (*s_point).getvch();
        else if (curve == CURVE25519)
            point_bytes = (*c_point).getvch();
        else if (curve == ED25519)
            point_bytes = (*e_point).getvch();
        bytes.resize(1 + point_bytes.size());
        bytes[0] = curve;
        memcpy(&bytes[1], &point_bytes[0], point_bytes.size());
        return bytes;
    }

    void Point::setvch(const vch_t& bytes)
    {
        if (bytes.size() == 0)
        {
            curve = SECP256K1;
            if (s_point == NULL)
                s_point = new Secp256k1Point();
            return;
        }
        curve = bytes[0];
        vch_t point_bytes(&bytes[1], &bytes[bytes.size()]);
        if (curve == SECP256K1)
        {
            if (s_point == NULL)
                s_point = new Secp256k1Point();
            (*s_point).setvch(point_bytes);
        }
        else if (curve == CURVE25519)
        {
            if (c_point == NULL)
                c_point = new Curve25519Point();
            (*c_point).setvch(point_bytes);
        }
        else if (curve == ED25519)
        {
            if (e_point == NULL)
                e_point = new Ed25519Point();
            (*e_point).setvch(point_bytes);
        }
    }


/*
 *  Point
 ***********/


Point operator+(const Point& a, const Point& b)
{
    Point r(a);
    r += b;
    return r;
}

Point operator-(const Point& a, const Point& b)
{
    Point r(a);
    r -= b;
    return r;
}

Point operator*(const Point& a, const CBigNum& n)
{
    Point r(a);
    r *= n;
    return r;
}

Point operator*(const CBigNum& n, const Point& a)
{
    return a * n;
}
