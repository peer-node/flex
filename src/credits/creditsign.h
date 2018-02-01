#ifndef TELEPORT_CREDITSIGN
#define TELEPORT_CREDITSIGN 1


#include <openssl/rand.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include "credits/Credit.h"
#include "credits/SignedTransaction.h"
#include "database/memdb.h"

#include "log.h"
#define LOG_CATEGORY "creditsign.h"

extern uint64_t numkeys;

inline uint160 KeyHash(Point key)
{
    vch_t vchKey = key.getvch();
    vch_t point_bytes(&vchKey[1], &vchKey[vchKey.size()]);
    return Hash160(point_bytes);
}

class CECKey
{
private:
    EC_KEY *pkey;

public:
    CECKey()
    {
        pkey = EC_KEY_new_by_curve_name(NID_secp256k1);
        assert(pkey != NULL);
    }

    ~CECKey()
    {
        EC_KEY_free(pkey);
    }

    void GetPubKey(vch_t &pubkey_bytes, bool fCompressed)
    {
        EC_KEY_set_conv_form(pkey, fCompressed ? POINT_CONVERSION_COMPRESSED : POINT_CONVERSION_UNCOMPRESSED);
        int nSize = i2o_ECPublicKey(pkey, NULL);
        assert(nSize);
        assert(nSize <= 65);
        unsigned char c[65];
        unsigned char *pbegin = c;
        int nSize2 = i2o_ECPublicKey(pkey, &pbegin);
        assert(nSize == nSize2);
        pubkey_bytes.resize(nSize);
        memcpy(&pubkey_bytes[0], &c[0], nSize);
    }

    bool SetPubKey(const vch_t &pubkey_bytes)
    {
        const unsigned char* pbegin = &pubkey_bytes[0];
        return o2i_ECPublicKey(&pkey, &pbegin, pubkey_bytes.size());
    }
};

inline uint160 FullKeyHash(Point pubkey)
{
    vch_t pubkey_bytes = pubkey.getvch();
    CECKey key;
    key.SetPubKey(pubkey_bytes);
    key.GetPubKey(pubkey_bytes, false);
    return Hash160(pubkey_bytes);
}


inline uint160 Rand160()
{
    uint160 r;
    RAND_bytes((uint8_t*)&r, 20);
    return r;
}

inline uint160 HashUint160(uint160 n)
{
    uint160 result = Hash160((uint8_t*)&n, (uint8_t*)&n + 20);
    return result;
}

inline Signature SignCreditTx(CBigNum privkey, CBigNum hash)
{
    return SignHashWithKey(hash.getuint256(), privkey, SECP256K1);
}

inline Point GetTransactionVerificationKey(UnsignedTransaction &unsigned_tx)
{
    uint256 hash = unsigned_tx.GetHash();
    Point key(SECP256K1, 0);
    uint32_t n = 0;

    for (uint32_t i = 0; i < unsigned_tx.inputs.size(); i++)
    {
        CreditInBatch batch_credit = unsigned_tx.inputs[i];
        hash = Hash(BEGIN(hash), END(hash));
        key = key + batch_credit.public_key * (CBigNum(hash) % key.Modulus());
    }
    return key;
}

inline Signature SignTx(UnsignedTransaction unsigned_tx, MemoryDataStore& keydata)
{
    uint256 hash_, hash = unsigned_tx.GetHash();
    CBigNum signing_key(0);

    CBigNum modulus = Point(SECP256K1, 0).Modulus();

    hash_ = hash;
    for (uint32_t i = 0; i < unsigned_tx.inputs.size(); i++)
    {
        CBigNum privkey = keydata[unsigned_tx.inputs[i].public_key]["privkey"];
        hash_ = Hash(BEGIN(hash_), END(hash_));
        signing_key = (signing_key + ((CBigNum(hash_) % modulus)) * privkey) % modulus;
    }
    Point pubkey(SECP256K1, signing_key);
    keydata[pubkey]["privkey"] = signing_key;
    
    Signature sig = SignCreditTx(signing_key, CBigNum(hash));
    
    return sig;
}

inline SignedTransaction SignTransaction(UnsignedTransaction unsigned_tx, MemoryDataStore& keydata)
{
    SignedTransaction tx;
    tx.rawtx = unsigned_tx;
    tx.signature = SignTx(unsigned_tx, keydata);
    return tx;
}

inline bool VerifyCreditTransferSignature(UnsignedTransaction unsigned_tx, Signature signature)
{
    Point verification_key = GetTransactionVerificationKey(unsigned_tx);
    uint256 hash = unsigned_tx.GetHash();
    
    return VerifySignatureOfHash(signature, hash, verification_key);
}

inline bool VerifyTransactionSignature(SignedTransaction tx)
{
    return VerifyCreditTransferSignature(tx.rawtx, tx.signature);
}


#endif