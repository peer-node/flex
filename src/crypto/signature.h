#ifndef FLEX_SIGNATURE
#define FLEX_SIGNATURE

#include "crypto/point.h"


class Signature
{
public:
    uint256 signature{0};
    uint256 exhibit{0};

    Signature() { }

    Signature(uint256 signature, uint256 exhibit): 
        signature(signature), 
        exhibit(exhibit)
    { }

    string_t ToString() const
    {
        std::stringstream ss;
        ss << "\n============== Signature =============" << "\n"
           << "== Exhibit: " << exhibit.ToString() << "\n"
           << "== Signature: " << signature.ToString() << "\n"
           << "============ End Signature ===========" << "\n";

        return ss.str();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(signature);
        READWRITE(exhibit);
    )

    JSON(signature, exhibit);

    bool operator==(const Signature& other) const
    {
        return signature == other.signature and exhibit == other.exhibit;
    }
};

inline uint256 HashUint256(uint256 n)
{
    uint256 result = Hash((uint8_t*)&n, (uint8_t*)&n + 32);
    return result;
}

inline uint256 GetExhibit(Point r, uint256 hash)
{
    CBigNum x, y;
    r.GetCoordinates(x, y);
    uint256 result = HashUint256(x.getuint256() + HashUint256(hash));
                    
    return result & ((uint256(1) << 252) - 1);
}

inline Signature SignHashWithKey(uint256 hash, CBigNum privkey, uint8_t curve)
{   
    CBigNum k(GetExhibit(Point(curve, CBigNum(hash)), privkey.getuint256()));

    Point r(curve, k);
    
    CBigNum exhibit(GetExhibit(r, hash));
 
    CBigNum signature = (k - (privkey * exhibit)) % r.Modulus();
  
    if (signature < 0)
        signature += r.Modulus();

    Signature sig(signature.getuint256(), exhibit.getuint256());

    return sig;
}

inline bool VerifySignatureOfHash(Signature signature, 
                                  uint256 hash, 
                                  Point pubkey)
{
    Point r(pubkey.curve, CBigNum(signature.signature));
    r += pubkey * CBigNum(signature.exhibit);

    return signature.exhibit == GetExhibit(r, hash);
}

#endif
