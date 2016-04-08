#ifndef FLEX_CREDITSIGN
#define FLEX_CREDITSIGN 1


#include <openssl/rand.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include "credits/Credit.h"
#include "credits/SignedTransaction.h"
#include "database/memdb.h"
#include "database/data.h"

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
    log_ << "GetTransactionVerificationKey() for " << unsigned_tx << "\n";
    
    for (uint32_t i = 0; i < unsigned_tx.inputs.size(); i++)
    {
        Point pubkey;
        CreditInBatch batch_credit = unsigned_tx.inputs[i];
        
        if (unsigned_tx.inputs[i].keydata.size() == 34)
        {
            pubkey.setvch(unsigned_tx.inputs[i].keydata);
        }
        else
        {
            pubkey = unsigned_tx.pubkeys[n];
            n++;
        }
        hash = Hash(BEGIN(hash), END(hash));
        key = key + pubkey * (CBigNum(hash) % key.Modulus());
    }
    log_ << "GetTransactionVerificationKey: returning " << key << "\n";
    return key;
}

inline Signature SignTx(UnsignedTransaction unsigned_tx)
{
    uint256 hash_, hash = unsigned_tx.GetHash();
    CBigNum signing_key(0);

    CBigNum modulus = Point(SECP256K1, 0).Modulus();

    hash_ = hash;
    for (uint32_t i = 0; i < unsigned_tx.inputs.size(); i++)
    {
        CBigNum privkey;
        if (unsigned_tx.inputs[i].keydata.size() == 20)
        {
            uint160 keyhash(unsigned_tx.inputs[i].keydata);
            log_ << "looking up privkey for keyhash " << keyhash << "\n";
            CBigNum privkey_ = keydata[keyhash]["privkey"];
            privkey = privkey_;
        }
        else
        {
            Point pubkey;
            pubkey.setvch(unsigned_tx.inputs[i].keydata);
            CBigNum privkey_ = keydata[pubkey]["privkey"];
            log_ << "looking for privkey for pubkey " << pubkey << "\n";
            privkey = privkey_;
        }
#ifndef _PRODUCTION_BUILD
        log_ << "SignTx: privkeykey is " << privkey << "\n";
#endif
        hash_ = Hash(BEGIN(hash_), END(hash_));
        signing_key = (signing_key + ((CBigNum(hash_) % modulus)) * privkey) 
                        % modulus;
    }
    Point pubkey(SECP256K1, signing_key);
    keydata[pubkey]["privkey"] = signing_key;
#ifndef _PRODUCTION_BUILD
    log_ << "signing key is " << signing_key << "\n";
    log_ << "corresponding public key is " << pubkey << "\n";
#endif
    
    Signature sig = SignCreditTx(signing_key, CBigNum(hash));
    
    return sig;
}

inline SignedTransaction SignTransaction(UnsignedTransaction unsigned_tx)
{
    SignedTransaction tx;
    tx.rawtx = unsigned_tx;
    tx.signature = SignTx(unsigned_tx);
    return tx;
}

inline bool CheckKeyHashes(UnsignedTransaction rawtx)
{
    uint32_t n = 0;

    for (uint32_t i = 0; i < rawtx.inputs.size(); i++)
    {
        if (rawtx.inputs[i].keydata.size() == 20)
        {
            if (n >= rawtx.pubkeys.size())
            {
                log_ << "CheckKeyHashes(): " << rawtx.pubkeys.size()
                     << " pubkeys but " << n + 1 << " 20-byte keys!\n";
                return false;
            }
            if (KeyHash(rawtx.pubkeys[n]) != uint160(rawtx.inputs[i].keydata))
            {
                log_ << "CheckKeyHashes(): "
                     << KeyHash(rawtx.pubkeys[n]) << " vs "
                     << uint160(rawtx.inputs[i].keydata) << "\n";
                return false;
            }
            n++;
        }
    }
    return true;
}

inline bool VerifyCreditTransferSignature(UnsignedTransaction unsigned_tx,
                                          Signature signature)
{
    Point verification_key = GetTransactionVerificationKey(unsigned_tx);
    log_ << "VerifyCreditTransferSignature(): verification_key is "
         << verification_key << "\n";
    
    uint256 hash = unsigned_tx.GetHash();
    
    return VerifySignatureOfHash(signature, hash, verification_key);

}

inline bool VerifyTransactionSignature(SignedTransaction tx)
{
    if (!CheckKeyHashes(tx.rawtx))
        return false;
    return VerifyCreditTransferSignature(tx.rawtx, tx.signature);
}


#endif