// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef TELEPORT_POINT
#define TELEPORT_POINT

#include "define.h"
#include "bignum.h"

#define FIAT 0
#define SECP256K1 1

class Secp256k1Point;

class Point
{
public:
    uint8_t curve{SECP256K1};
    Secp256k1Point *s_point;

    Point();

    Point(uint8_t curve, CBigNum n);

    Point(CBigNum n);

    Point(const Point& other);

    Point& operator=(const Point& other);

    ~Point();

    string_t ToString() const;

    std::string json()
    {
        return "\"" + ToString() + "\"";
    }

    void SetToMultipleOfGenerator(CBigNum n);

    void GetCoordinates(CBigNum &x, CBigNum &y) const;

    void SetToInfinity();

    bool IsAtInfinity() const;

    bool operator!() const;

    bool operator<(const Point& other) const;

    operator vch_t();

    bool operator==(const Point& other) const;

    bool operator!=(const Point& other) const;

    Point& operator+=(const Point& other);

    Point& operator-=(const Point& other);

    Point& operator*=(const CBigNum &n);

    Point& operator*=(const uint256 &n);

    CBigNum Modulus() const;

    vch_t getvch() const;

    bool setvch(const vch_t& bytes);

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

Point operator+(const Point& a, const Point& b);

Point operator-(const Point& a, const Point& b);

Point operator*(const Point& a, const CBigNum& n);

Point operator*(const CBigNum& n, const Point& a);

Point operator*(const Point& a, const uint256& n);

Point operator*(const uint256& n, const Point& a);



#endif