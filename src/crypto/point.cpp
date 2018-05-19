#include "crypto/secp256k1point.h"
#include "crypto/point.h"

#define FIAT 0
#define SECP256K1 1

/***********
 *  Point
 */

    Point::Point():
        curve(SECP256K1)
    { s_point = new Secp256k1Point(); }

    Point::Point(uint8_t curve, CBigNum n):
        curve(curve),
        s_point(NULL)
    {
        if (curve == SECP256K1)
            s_point = new Secp256k1Point();

        if (n != 0)
            SetToMultipleOfGenerator(n);
    }

    Point::Point(CBigNum n):
        curve(SECP256K1)
    {
        s_point = new Secp256k1Point();
        if (n != 0)
            SetToMultipleOfGenerator(n);
    }

    Point::Point(const Point& other):
        curve(other.curve),
        s_point(NULL)
    {
        curve = other.curve;
        if (curve == SECP256K1 && other.s_point != NULL)
        {
            s_point = new Secp256k1Point();
            *s_point = *other.s_point;
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
        return (*this);
    }

    Point::~Point()
    {
        if (s_point != NULL)
            delete s_point;
    }

    string_t Point::ToString() const
    {
        if (curve == SECP256K1)
            return (*s_point).ToString();
        return string_t();
    }

    void Point::SetToMultipleOfGenerator(CBigNum n)
    {
        if (curve == SECP256K1)
            (*s_point).SetToMultipleOfGenerator(n);
    }

    void Point::GetCoordinates(CBigNum &x, CBigNum &y) const
    {
       if (curve == SECP256K1)
            (*s_point).GetCoordinates(x, y);
    }

    void Point::SetToInfinity()
    {
        if (curve == SECP256K1)
            (*s_point).SetToInfinity();
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
        return vch_t();
    }

    bool Point::operator==(const Point& other) const
    {
        if (curve == SECP256K1 and other.curve == SECP256K1)
            return *s_point == *other.s_point;
        return false;
    }

    bool Point::operator!=(const Point& other) const
    {
        return !(*this == other);
    }

    Point& Point::operator+=(const Point& other)
    {
        if (curve == SECP256K1 and other.curve == SECP256K1)
            *s_point += *other.s_point;
        return (*this);
    }

    Point& Point::operator-=(const Point& other)
    {
        if (curve == SECP256K1 and other.curve == SECP256K1)
            *s_point -= *other.s_point;
        return (*this);
    }

    Point& Point::operator*=(const CBigNum &n)
    {
        if (curve == SECP256K1)
            *s_point *= n;
        return (*this);
    }

    CBigNum Point::Modulus() const
    {
        if (curve == SECP256K1)
            return Secp256k1Point::Modulus();
        return CBigNum(0);
    }

    vch_t Point::getvch() const
    {
        vch_t point_bytes, bytes;
        if (curve == SECP256K1)
            point_bytes = (*s_point).getvch();
        bytes.resize(1 + point_bytes.size());
        bytes[0] = curve;
        memcpy(&bytes[1], &point_bytes[0], point_bytes.size());
        return bytes;
    }

    bool Point::setvch(const vch_t& bytes)
    {
        if (bytes.size() == 0)
        {
            curve = SECP256K1;
            if (s_point == NULL)
                s_point = new Secp256k1Point();
            return false;
        }
        curve = bytes[0];
        vch_t point_bytes(&bytes[1], &bytes[bytes.size()]);
        if (curve == SECP256K1)
        {
            if (s_point == NULL)
                s_point = new Secp256k1Point();
            return (*s_point).setvch(point_bytes);
        }
        return false;
    }

Point &Point::operator*=(const uint256 &n)
{
    return *this *= CBigNum(n);
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

Point operator*(const Point& a, const uint256& n)
{
    return a * CBigNum(n);
}

Point operator*(const uint256& n, const Point& a)
{
    return a * CBigNum(n);
}


