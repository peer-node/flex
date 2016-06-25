#ifndef FLEX_25519_POINTS
#define FLEX_25519_POINTS
#include <stdio.h>
#include "crypto/25519/ed25519.h"
#include "crypto/25519/ed25519-donna.h"
#include "crypto/bignum.h"

#include "log.h"
#define LOG_CATEGORY "25519points.h"


static const uint8_t ed25519_order[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

static void BigNum256ModMFromBigNum(bignum25519 bignum, CBigNum number)
{
    memset(bignum, 0, 40);
    CBigNum twopow56(1);
    twopow56 <<= 56;
    for (uint32_t i = 0; i < 5; i++)
    {
        bignum[i] = (number % twopow56).getulong();
        number >>= 56;
    }
}

inline CBigNum F25519ToBigNum(const uint8_t *x)
{
    CBigNum number(0);
    for (int32_t i = 32 - 1; i >=0 ; i--)
    {
        number <<= 8;
        uint64_t n = x[i];
        number = number + CBigNum(n);
    }
    return number;
}

static string_t bignum25519toString(const bignum25519 bignum)
{
    CBigNum n = bignum25519toCBigNum(bignum);
    return n.ToString();
}

class Curve25519Point;

class Ed25519Point
{
public:
    ge25519 ALIGN(16) group_element;

    Ed25519Point()
    { }

    string_t ToString() const
    {
        string_t output;
        bignum25519 ALIGN(16) ex, ey;
        ed25519_unproject(ex, ey, &group_element);
        output = bignum25519toString(ex) + "," + bignum25519toString(ey);
        return output;
    }

    void Normalise()
    {
        bignum25519 ALIGN(16) ex, ey;
        ed25519_unproject(ex, ey, &group_element);
        ed25519_project(&group_element, ex, ey);
    }

    inline Ed25519Point(Curve25519Point montgomery);

    Ed25519Point(const Ed25519Point& other)
    {
        memcpy(&group_element, &other.group_element, 160);
    }

    Ed25519Point(CBigNum number)
    {
        if (number < 100000000)
            SetToMultipleOfGenerator(number);
        else
        {
            SetToMultipleOfGenerator(1);
            DoStupidMultiplication(number);
        }
    }

    void DoStupidMultiplication(const CBigNum& n)
    {
        CBigNum number = n;
        Ed25519Point p = (*this);
        Ed25519Point q(CBigNum(0));

        while (number != 0)
        {
            if (number % 2 == 1)
                q += p;
            p += p;
            number = number / 2;
        }
        (*this) = q;
    }

    Ed25519Point& operator=(const Ed25519Point& other)
    {
        memcpy(&group_element, &other.group_element, 160);
        return (*this);
    }

    void GetCoordinates(CBigNum &x, CBigNum &y)
    {
        bignum25519 ALIGN(16) ex, ey;
        ed25519_unproject(ex, ey, &group_element);
        x = bignum25519toCBigNum(ex);
        y = bignum25519toCBigNum(ey);
    }

    bool operator==(const Ed25519Point& other) const
    {
        ed25519_public_key k1, k2;
        ge25519_pack(k1, &group_element);
        ge25519_pack(k2, &other.group_element);
        
        return memcmp(k1, k2, 32) == 0;
    }

    bool operator!=(const Ed25519Point& other) const
    {
        return memcmp(&group_element, &other.group_element, 160);
    }

    Ed25519Point& operator+=(const Ed25519Point& other)
    {
        ge25519 old_element;
        Normalise();
        memcpy(&old_element, &group_element, 160);

        ge25519_add(&group_element, &old_element, &other.group_element);
        Normalise();
        return (*this);
    }

    Ed25519Point operator-() const
    {
        Ed25519Point result;
        result = *this;
        bignum25519 x, y;
        ed25519_unproject(x, y, &result.group_element);
        curve25519_neg(y, y);
        ed25519_project(&result.group_element, x, y);
        return result;
    }

    Ed25519Point& operator-=(const Ed25519Point& other)
    {
        (*this) += (-other);
        return *this;
    }

    Ed25519Point& operator*=(const bignum25519 number)
    {
        bignum25519 number_modm;
        bignum256modm zero;
        memset(&zero, 0, 40);
        ge25519 old_element;
        Normalise();
        memcpy(&old_element, &group_element, 160);

        unsigned char r[32];
        contract256_modm(r, number);
        expand256_modm(number_modm, r, 32);
        ge25519_double_scalarmult_vartime(&group_element, &old_element,
                                          number_modm, zero);
        Normalise();
        return (*this);
    }

    Ed25519Point& operator*=(const CBigNum& n)
    {
        DoStupidMultiplication(n);
        return (*this);
    }

    bool operator!()
    {
        bignum256modm zero;
        memset(&zero, 0, 40);
        return memcmp(&group_element, &zero, 40);
    }


    Ed25519Point& SetToMultipleOfGenerator(CBigNum number)
    {
        if (number < 100000000)
        {
            bignum256modm number_;
            CBigNumtobignum25519(number_, number);
            ge25519_scalarmult_base_niels(&group_element, 
                                          ge25519_niels_base_multiples,
                                          number_);
        }
        else
        {
            SetToMultipleOfGenerator(1);
            DoStupidMultiplication(number);
        }
        
        return *this;
    }

    Ed25519Point& SetToInfinity()
    {
        return SetToMultipleOfGenerator(0);
    }

    static CBigNum Modulus()
    {
        return F25519ToBigNum(ed25519_order);
    }

    vch_t getvch() const
    {
        ed25519_public_key key;
        ge25519_pack(key, &group_element);

        key[31] ^= (1 << 7);

        vch_t bytes((unsigned char*)&key, (unsigned char*)&key + 32);
        return bytes;
    }

    bool setvch(const vch_t& bytes)
    {
        if (bytes.size() != 32)
            return false;
        ed25519_public_key key;
        memcpy(&key, &bytes[0], 32);
        ge25519_unpack_negative_vartime(&group_element, key);
        return true;
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


inline Ed25519Point operator*(const CBigNum& bn, const Ed25519Point& p)
{
    Ed25519Point r;
    r = p;
    r *= bn;
    return r;
}


inline Ed25519Point operator+(const Ed25519Point& q, const Ed25519Point& p)
{
    Ed25519Point r;
    r = q;
    r += p;
    return r;
}


inline Ed25519Point operator-(const Ed25519Point& q, const Ed25519Point& p)
{
    Ed25519Point r;
    r = q;
    r -= p;
    return r;
}

class Curve25519Point
{
public:
    curved25519_key x;
    int parity;

    Curve25519Point()
    { 
        parity = 0;
    }

    string_t ToString() const
    {
        string_t output;
        bignum25519 ALIGN(16) s;
        curve25519_expand(s, x);
        char p[3];
        sprintf(p, "%d", parity);
        output = bignum25519toString(s) + string_t(",") + string_t(p);
        return output;
    }

    Curve25519Point(Ed25519Point ep)
    {
        bignum25519 ALIGN(16) x_out, ex, ey;
        morph25519_e2m(x_out, ep.group_element.y, ep.group_element.z);
        mod25519(x_out);
        curve25519_contract(x, x_out);
        ed25519_unproject(ex, ey, &ep.group_element);
        parity = morph25519_eparity(ex);
    }

    void GetCoordinates(CBigNum &x_, CBigNum &y_)
    {
        bignum25519 ALIGN(16) mx;
        curve25519_expand(mx, x);
        
        x_ = bignum25519toCBigNum(mx);
        y_ = parity;
    }

    Curve25519Point(CBigNum number)
    {
       SetToMultipleOfGenerator(number);
    }

    Curve25519Point& operator=(const Curve25519Point& other)
    {
        memcpy(&x, &other.x, 32);
        parity = other.parity;
        return (*this);
    }

    Curve25519Point operator-() const
    {
        Curve25519Point result;
        result = *this;
        result.parity = 1 - result.parity;
        return result;
    }

    bool operator==(const Curve25519Point& other) const
    {
        return parity == other.parity && !memcmp(&x, &other.x, 32);
    }

    bool operator!=(const Curve25519Point& other) const
    {
        return !(*this == other);
    }

    Curve25519Point& operator+=(const Curve25519Point& other)
    {
        curved25519_add(x, &parity, x, parity, other.x, other.parity);
        return (*this);
    }

    Curve25519Point& operator-=(const Curve25519Point& other)
    {
        (*this) += (-other);
        return (*this);
    }

    Curve25519Point& operator*=(const CBigNum& n)
    {
        Ed25519Point ed(*this);
        ed *= n;
        (*this) = Curve25519Point(ed);
        return (*this);
    }

    Curve25519Point& SetToMultipleOfGenerator(CBigNum number)
    {
        Ed25519Point ed(number);
        (*this) = Curve25519Point(ed);
        return (*this);
    }

    Curve25519Point& SetToInfinity()
    {
        return SetToMultipleOfGenerator(0);
    }

    static CBigNum Modulus()
    {
        return F25519ToBigNum(ed25519_order);
    }

    vch_t getvch() const
    {
        vch_t bytes;
        bytes.resize(33);
        bytes[32] = parity;
        memcpy(&bytes[0], x, 32);
        return bytes;
    }

    bool setvch(const vch_t& bytes)
    {
        if (bytes.size() != 33)
            return false;
        memcpy(x, &bytes[0], 32);
        parity = bytes[32];
        return true;
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


Ed25519Point::Ed25519Point(Curve25519Point montgomery)
{
    bignum25519 ALIGN(16) ex, ey, mx;
    curve25519_expand(mx, montgomery.x);
    morph25519_m2e(ex, ey, mx, montgomery.parity);
    mod25519(ex);
    mod25519(ey);
    ed25519_project(&group_element, ex, ey);
}

inline vch_t Hash160ToVch(uint160 hash)
{
    vch_t text;
    text.resize(20);
    memcpy(&text[0], &hash, 20);
    return text;
}


#endif
