#ifndef FLEX_DATASTORE
#define FLEX_DATASTORE

#include "leveldbwrapper.h"
#include <stdint.h>
#include "base/serialize.h"
#include <string.h>
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <typeinfo>
#include <sys/time.h>
#include "define.h"
#include <boost/shared_ptr.hpp>
#include <boost/utility/value_init.hpp>
#include "base/allocators.h"
#include <openssl/rand.h>
#include "base/util.h"
#include "crypto/uint256.h"
#include "crypto/hash.h"


#include "log.h"
#define LOG_CATEGORY "datastore.h"

template <typename T> 
inline vch_t Bytes(T thing)
{
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << thing;
    return vch_t(ss.begin(), ss.end());
}

template <typename T> 
inline T ObjectFromBytes(vch_t bytes)
{
    T thing;
    CDataStream ss(bytes.begin(), bytes.end(), SER_NETWORK, CLIENT_VERSION);
    ss >> thing;
    return thing;
}


class CPrefixDBBatch : public CLevelDBBatch
{
public:
    string_t strPrefix;
    CPrefixDBBatch(const char *pchPrefix)
    {
        strPrefix = string_t(pchPrefix);
    }
    template <typename K, typename V> void Write(const K& key, const V& value)
    {
        CLevelDBBatch::Write(std::make_pair(strPrefix, key), value);
    }
    template <typename K> void Erase(const K& key)
    {
        CLevelDBBatch::Erase(std::make_pair(strPrefix, key));
    }
};

class CPrefixDBIterator
{
public:
    string_t strPrefix;
    boost::shared_ptr<leveldb::Iterator> it;

    CPrefixDBIterator()
    {
    }

    CPrefixDBIterator(CLevelDBWrapper *pdb, string_t strPrefixIn)
    : strPrefix(strPrefixIn)
    {
        it = boost::shared_ptr<leveldb::Iterator>(pdb->NewIterator());
        Seek(string_t("1"));
    }

    template <typename K> void 
    Seek(K key)
    {
        leveldb::Slice seekSlice;
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << std::make_pair(strPrefix, key);
        string_t strSeek(ss.begin(), ss.end());
        seekSlice = strSeek;
        it->SeekToFirst();
        it->Seek(seekSlice);
    }

    template <typename K, typename V> bool 
    GetNextKeyAndValue(K& key, V& value)
    {
        try
        {
            std::pair<string_t, string_t> pairKey;
            string_t strKey = it->key().ToString();
            string_t strValue = it->value().ToString();
            CDataStream ss(&strKey[0], &strKey[0] + strKey.size(),
                           SER_DISK, CLIENT_VERSION);
            ss >> pairKey;
            if (pairKey.first != strPrefix)
                return false;
            key = pairKey.second;
            ss = CDataStream(&strValue[0], &strValue[0] + strValue.size(), 
                             SER_DISK, CLIENT_VERSION);
            ss >> value;
            return true;
        } 
        catch (...) 
        { 
            return false; 
        }    
    }
};

class CPrefixDB
{
public:
    CLevelDBWrapper *pdb;
    string_t strPrefix;

    CPrefixDB() { }

    CPrefixDB(CLevelDBWrapper *pdbIn, const char *pchPrefix) : pdb(pdbIn)
    {
        strPrefix = string_t(pchPrefix);
    }

    template <typename V> bool
    Write(char pchKey[], const V& value, bool fSync = false) throw(leveldb_error)
    {
        return pdb->Write(std::make_pair(strPrefix, string_t(pchKey)), 
                          value, fSync);
    }

    template <typename K, typename V> bool
    Write(const K& key, const V& value, bool fSync = false) throw(leveldb_error)
    {
        return pdb->Write(std::make_pair(strPrefix, key), value, fSync);
    }

    template <typename V> bool 
    Read(const char *pchKey, V& value) throw(leveldb_error)
    {
        return Read(string_t(pchKey), value);
    }

    template <typename K, typename V> bool 
    Read(const K& key, V& value) throw(leveldb_error)
    {
        return pdb->Read(std::make_pair(strPrefix, key), value);
    }

    template <typename K> bool 
    Erase(const K& key, bool fSync = true) throw(leveldb_error)
    {
        return pdb->Erase(std::make_pair(strPrefix, key), fSync);
    }

    template <typename K> bool 
    Exists(const K& key) throw(leveldb_error) 
    {
        return pdb->Exists(std::make_pair(strPrefix, key));
    }

    CPrefixDBBatch Batch()
    {
        return CPrefixDBBatch(strPrefix.c_str());
    }

    bool 
    WriteBatch(CPrefixDBBatch &batch, bool fSync = false) throw(leveldb_error)
    {
        return pdb->WriteBatch(batch, fSync);
    }

    bool Sync() throw(leveldb_error) 
    {
        CLevelDBBatch batch;
        return pdb->WriteBatch(batch, true);
    }

    CPrefixDBIterator Iterator()
    {
        return CPrefixDBIterator(pdb, strPrefix);
    }

    bool IsKey(leveldb::Slice key) throw(leveldb_error)
    {
        string_t strKey = key.ToString();
        std::pair<string_t, vch_t> pairKey;
        CDataStream ss(&strKey[0], &strKey[0] + strKey.size(),
                       SER_DISK, CLIENT_VERSION);
        ss >> pairKey;
        return pairKey.first == strPrefix && pdb->Exists(pairKey);
    }
};


class CDataStore;

template <typename K, typename N>
class CProperty;

template <typename K, typename N>
class CLocation;

template <typename N=string_t>
class CObject 
{ 
public:
    CDataStore &db;
    const N& objectname;
    string_t strLocalName;  /* A name that won't go out of scope */
    bool fUsingLocalName;

    CObject(CDataStore &db, const N& objectname)
    : db(db), objectname(objectname)
    { 
        fUsingLocalName = false;
    }

    CObject(CDataStore &db, const string_t strName, bool fUseLocalName)
    : db(db), objectname(strName)
    { 
        strLocalName = objectname;
        fUsingLocalName = fUseLocalName;
    }

    IMPLEMENT_SERIALIZE
    (
        if (fUsingLocalName)
            READWRITE(strLocalName);
        else
            READWRITE(objectname);
    )

    template <typename K>
    CProperty<K,N> operator [] (K& key) 
    {
        return CProperty<K,N>(*this, key);
    }

    CProperty<string_t,N> operator [] (const char* pchKey) 
    {
        return CProperty<string_t,N>(*this, pchKey);
    }

    template <typename K>
    bool HasProperty(K& key);

    bool HasProperty(const char* key);

    CLocation<string_t,N>  Location(const char* pchKey)
    {
        return CLocation<string_t,N>(*this, pchKey);
    }

    template <typename K>
    CLocation<K,N>  Location(const K& key) 
    {
        return CLocation<K,N>(*this, key);
    }
};




template <typename N>
class CPropertyIterator
{
public:
    CDataStore *pds;
    CPrefixDBIterator it;
    N objectname;

    CPropertyIterator(){};

    CPropertyIterator(CDataStore *pdsIn, N objectnameIn);

    template <typename P, typename V> bool 
    GetNextPropertyAndValue(P& propertyname, V& value);
};

template <typename L=string_t>
class CLocationIterator
{
public:
    CDataStore *pds;
    CPrefixDBIterator it;
    L locationname;

    CLocationIterator(){};

    CLocationIterator(CDataStore *pdsIn, L locationnameIn);

    template <typename V> void
    Seek(V locationvalue);

    template <typename V> void
    Seek(V locationvalue, bool fReverse);

    void 
    SeekStart()
    {
        Seek((uint8_t)0);
    }

    void 
    SeekEnd()
    {
        Seek((int64_t)(-1), true);
    }

    template <typename N, typename V> bool
    GetNextObjectAndLocation(N& objectname, V& locationvalue);

    template <typename N, typename V> bool
    GetNextObjectAndLocation(N& objectname, V& locationvalue, bool fReverse);
    
    template <typename N, typename V> bool 
    GetPreviousObjectAndLocation(N& objectname, V& locationvalue);
};

typedef std::vector<unsigned char, secure_allocator<unsigned char> > svch_t;

class CDataStore : public CPrefixDB
{
private:
    uint32_t nNextNumber;
    bool encrypted;
    svch_t *passphrase_hash;

public:
    CDataStore() { }

    CDataStore(CLevelDBWrapper *pdb, const char *pchPrefix)
    : CPrefixDB(pdb, pchPrefix)
    {
        nNextNumber = NextNumber();
        encrypted = false;
    }

    void SetEncryptionKey(const vch_t& passphrase)
    {
        passphrase_hash = new svch_t();
        (*passphrase_hash).resize(32);
        uint256 hash = Hash(UBEGIN(passphrase), UEND(passphrase));
        memcpy(&(*passphrase_hash)[0], UBEGIN(hash), 32);
    }

    void ClearEncryptionKey()
    {
        delete passphrase_hash;
        passphrase_hash = NULL;
    }

     ~CDataStore()
     {
        if (passphrase_hash != NULL)
            delete passphrase_hash;
     }

    uint32_t NextNumber()
    {
        uint32_t nNumber = 0;
        Read(0, nNumber);
        Write(0, nNumber + 1);
        return nNumber + 1;
    }

    uint32_t Id(const char *pchKey)
    {
        return Id(string_t(pchKey));
    }

    template <typename K>
    uint32_t Id(const K& key)
    {
        uint32_t nNumber = 0;
        if (!Read(std::make_pair('k', key), nNumber))
        {
            nNumber = NextNumber();
            Write(std::make_pair('k', key), nNumber);
            Write(std::make_pair('n', nNumber), key);
        }
        return nNumber;
    }

    template <typename K> bool
    GetKey(uint32_t nNumber, K& key)
    {
        return Read(std::make_pair('n', nNumber), key);
    }

    template <typename K, typename V, typename N> bool
    SetProperty(const K& key, const N& propertyname, 
                const V& value, 
                bool fSync = false) throw(leveldb_error)
    {
        if (encrypted)
        {
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            ssValue << value;
            vch_t bytes(ssValue.begin(), ssValue.end());

            uint256 salt = GetSalt();

            std::pair<uint256, vch_t> encrypted_value
                = std::make_pair(salt, EncryptWithSalt(bytes, 
                                                       *passphrase_hash, salt));

            return Write(std::make_pair(std::make_pair('p', Id(key)), 
                                        Id(propertyname)), 
                         encrypted_value, fSync);
        }
        else
            return Write(std::make_pair(std::make_pair('p', Id(key)), 
                               Id(propertyname)), 
                         value, fSync);
    }

    template <typename K, typename V, typename N> bool
    GetProperty(const K& key, const N& propertyname, 
                V& value) throw(leveldb_error)
    {
        if (encrypted)
        {
            std::pair<uint256, vch_t> encrypted_value;
            if (!Read(std::make_pair(std::make_pair('p', Id(key)),
                                     Id(propertyname)), encrypted_value))
                return false;

            uint256 salt = encrypted_value.first;
            vch_t encrypted_bytes = encrypted_value.second;
            vch_t bytes = DecryptWithSalt(encrypted_bytes, *passphrase_hash, salt);

            CDataStream ss(bytes, SER_DISK, CLIENT_VERSION);

            try
            {
                ss >> value;
            } 
            catch (...)
            {
                return false;
            }
            return true;
        }
        else
            return Read(std::make_pair(std::make_pair('p', Id(key)),
                                       Id(propertyname)), value);
    }

    template <typename K, typename N> bool
    HasProperty(const K& key, const N& propertyname) throw(leveldb_error)
    {
        return Exists(std::make_pair(std::make_pair('p', Id(key)),
                                Id(propertyname)));
    }

    template <typename K> bool
    HasProperty(const K& key, const char* pchProperty) throw(leveldb_error)
    {
        return Exists(std::make_pair(std::make_pair('p', Id(key)),
                                     Id(string_t(pchProperty))));
    }

    template <typename K, typename N> bool
    EraseProperty(const K& key, const N& propertyname) throw(leveldb_error)
    {
        return Erase(std::make_pair(
                     std::make_pair('p', Id(key)), Id(propertyname)));
    }

    template <typename K, typename N> bool
    EraseProperty(const K& key, const char* pchProperty) throw(leveldb_error)
    {
        return Erase(std::make_pair(
                     std::make_pair('p', Id(key)), Id(string_t(pchProperty))));
    }

    template <typename K> 
    CPropertyIterator<K> PropertyIterator(const K& key)
    {
        return CPropertyIterator<K>(this, key);
    }

    template <typename K, typename V, typename L> bool
    SetLocation(const K& key, const L& locationname, const V& value,
                bool fSync = false) throw(leveldb_error)
    {
        SetProperty(key, locationname, value);
        return Write(std::make_pair(std::make_pair('l', Id(locationname)), 
                                    ReverseBytes(value)),
                     Id(key), fSync);
    }

    template <typename K, typename V, typename L> bool
    GetLocation(const K& key, const L& locationname, 
                V& value) throw(leveldb_error)
    {
        V value_;
        bool result = GetProperty(key, locationname, value_);
        value = ReverseBytes(value_);
        return result;
    }

    template <typename V, typename L> bool
    RemoveFromLocation(const L& locationname, const V& value)
    {
        return Erase(std::make_pair(std::make_pair('l', Id(locationname)), 
                                    ReverseBytes(value)));
    }

    template <typename K, typename V, typename N> bool
    GetObjectAtLocation(const V& locationvalue,
                        const N& locationname,
                        K& key) throw(leveldb_error)
    {
        uint32_t keyid;
        if (!Read(std::make_pair(std::make_pair('l', Id(locationname)), 
                                   ReverseBytes(locationvalue)), keyid))
            return false;
        return GetKey(keyid, key);
    }

    CLocationIterator<string_t> LocationIterator(const char* locationname)
    {
        return CLocationIterator<string_t>(this, string_t(locationname));
    }

    template <typename L>
    CLocationIterator<L> LocationIterator(L locationname)
    {
        return CLocationIterator<L>(this, locationname);
    }

    CObject<string_t> Object(const char* pchName)
    {
        return CObject<string_t>(*this, string_t(pchName), true);
    }

    CObject<string_t> operator [] (const char* pchName)
    {
        return Object(pchName);
    }

    template <typename N>
    CObject<N> Object(const N& objectname)
    {
        return CObject<N>(*this, objectname);
    }

    template <typename N>
    CObject<N> operator [] (const N& objectname)
    {
        return Object(objectname);
    }

    void Delete(const char *pchName)
    {
        Delete(string_t(pchName));
    }

    template <typename N>
    void Delete(N objectname)
    {        
        uint32_t nObject_id = Id(objectname);
        CPropertyIterator<N> it_ = PropertyIterator(objectname);
        boost::shared_ptr<leveldb::Iterator> it = it_.it.it;
        for (; it->Valid(); it->Next())
        {
            string_t strKey = it->key().ToString();
            CDataStream ss(&strKey[0], &strKey[0] + strKey.size(),
                           SER_DISK, CLIENT_VERSION);
            std::pair<string_t, std::pair<std::pair<char, uint32_t>, uint32_t> > pairKey;
            try
            {
                ss >> pairKey;
                if (pairKey.first != strPrefix ||
                    pairKey.second.first != std::make_pair('p', nObject_id))
                    break;
                uint32_t nProperty_id = pairKey.second.second;
                Erase(std::make_pair(std::make_pair('p', nObject_id), nProperty_id));
            } catch(...) { }
            
            // if this property is a location, remove object from it
            uint32_t nLocation_id = pairKey.second.second;
            string_t strVal = it->value().ToString();
            std::reverse(strVal.begin(), strVal.end()); // checkme
            CFlatData value(&strVal[0], &strVal[0] + strVal.size());
            uint32_t nObject_at_location_id = 0;

            if (Read(std::make_pair(std::make_pair('l', nLocation_id), value), 
                     nObject_at_location_id) && 
                nObject_id == nObject_at_location_id)
            {
                Erase(std::make_pair(std::make_pair('l', nLocation_id), value));
            }
        }  
    }

    uint256 HashU256(uint256 n)
    {
        uint256 result = Hash((uint8_t*)&n, (uint8_t*)&n + 32);
        return result;
    }

    uint256 GetSalt()
    {
        uint256 result;
        RAND_bytes(UBEGIN(result), 32);
        return result;
    }

    vch_t GetByteStream(uint32_t length, svch_t &seed)
    {
        vch_t result;
        uint256 num = Hash(seed.begin(), seed.end());
        while (result.size() < length)
        {
            num = HashU256(num);
            result.resize(result.size() + 32);
            memcpy(&result[result.size() - 32], UBEGIN(num), 32);
        }
        result.resize(length);
        return result;
    }

    vch_t XorBytes(vch_t bytes1, vch_t bytes2)
    {
        for (uint32_t i = 0; i < bytes1.size(); i++)
            bytes1[i] ^= bytes2[i];
        return bytes1;
    }

    vch_t EncryptWithSalt(vch_t bytes, svch_t &secret, uint256 salt)
    {
        vch_t saltbytes;
        saltbytes.resize(32);
        memcpy(&saltbytes[0], UBEGIN(salt), 32);
        secret.insert(secret.end(), saltbytes.begin(), saltbytes.end());
        vch_t mask = GetByteStream(bytes.size(), secret);
        secret.resize(32);
        return XorBytes(mask, bytes);
    }

    vch_t DecryptWithSalt(vch_t bytes, svch_t &secret, uint256 salt)
    {
        return EncryptWithSalt(bytes, secret, salt);
    }

    template <typename T> T ReverseBytes(const T& in) const
    {
        T out;
        for (uint32_t i = 0; i < sizeof(in); i++)
            memcpy(BEGIN(out) + i, END(in) - i - 1, 1);
        return out;
    }
};

template <typename N>
template <typename K>
bool CObject<N>::HasProperty(K& key)
{
    return db.HasProperty(*this, key);
}

template <typename N>
bool CObject<N>::HasProperty(const char* key)
{
    return db.HasProperty(*this, string_t(key));
}

template <typename N>
CPropertyIterator<N>::CPropertyIterator(CDataStore *pdsIn, N objectnameIn)
{
    pds = pdsIn;
    objectname = objectnameIn;
    it = pds->Iterator();
    it.Seek(std::make_pair(std::make_pair('p', pds->Id(objectname)), 0));
}

template <typename N>
template <typename P, typename V> bool 
CPropertyIterator<N>::GetNextPropertyAndValue(P& propertyname, V& value)
{
    leveldb::Iterator *pit = it.it;
    try
    {
        pit->Next();
        if (!pit->Valid())
            return false;
        std::pair<string_t, 
                std::pair<std::pair<char,uint32_t>,uint32_t> > pairKey;
        string_t strKey = pit->key().ToString();
        string_t strValue = pit->value().ToString();
        CDataStream ss(&strKey[0], &strKey[0] + strKey.size(),
                       SER_DISK, CLIENT_VERSION);
        ss >> pairKey;
        if (pairKey.first != it.strPrefix ||
            pairKey.second.first != std::make_pair('p', pds->Id(objectname)))
            return false;
        pds->GetKey(pairKey.second.second, propertyname);
        ss = CDataStream(&strValue[0], &strValue[0] + strValue.size(),
                         SER_DISK, CLIENT_VERSION);
        ss >> value;
        return true;
    } 
    catch (...)
    { 
        return false;
    }
}

template <typename L>
CLocationIterator<L>::CLocationIterator(CDataStore *pdsIn, L locationnameIn)
{
    pds = pdsIn;
    locationname = locationnameIn;
    it = pds->Iterator();
    it.Seek(std::make_pair('l', pds->Id(locationname)));
}

template <typename L>
template <typename N, typename V> bool 
CLocationIterator<L>::GetNextObjectAndLocation(N& objectname, 
                                               V& locationvalue,
                                               bool fReverse)
{
    boost::shared_ptr<leveldb::Iterator> pit = it.it;
    try
    {
        if (!pit->Valid())
            return false;
        std::pair<string_t, 
                std::pair<std::pair<char,uint32_t>,V> > pairKey;
        string_t strKey = pit->key().ToString();
        string_t strValue = pit->value().ToString();
        if (fReverse)
            pit->Prev();
        else
            pit->Next();

        if (strKey.find('l') == std::string::npos)
            return false;

        CDataStream ss(&strKey[0], &strKey[0] + strKey.size(),
                       SER_DISK, CLIENT_VERSION);

        ss >> pairKey;

        if (pairKey.first != it.strPrefix ||
            pairKey.second.first != std::make_pair('l', pds->Id(locationname)))
            return false;

        locationvalue = pds->ReverseBytes(pairKey.second.second);

        uint32_t nObjectId;
        ss = CDataStream(&strValue[0], &strValue[0] + strValue.size(),
                         SER_DISK, CLIENT_VERSION);
        ss >> nObjectId;
        pds->GetKey(nObjectId, objectname);
        return true;
    } 
    catch (...)
    { 
        return false;
    }
}

template <typename L>
template <typename N, typename V> bool 
CLocationIterator<L>::GetPreviousObjectAndLocation(N& objectname, 
                                                   V& locationvalue)
{
    return GetNextObjectAndLocation(objectname, locationvalue, true);
}

template <typename L>
template <typename N, typename V> bool 
CLocationIterator<L>::GetNextObjectAndLocation(N& objectname, 
                                               V& locationvalue)
{
    return GetNextObjectAndLocation(objectname, locationvalue, false);
}


template <typename L>
template <typename V> void
CLocationIterator<L>::Seek(V locationvalue, bool fReverse)
{
    it.Seek(std::make_pair(std::make_pair('l', pds->Id(locationname)), 
                           pds->ReverseBytes(locationvalue)));
    if (fReverse && it.it->Valid())
    {
        it.it->Prev();
    }
}

template <typename L>
template <typename V> void
CLocationIterator<L>::Seek(V locationvalue)
{
    Seek(locationvalue, false);
}

template <typename K, typename N>
class CProperty
{
public:
    CObject<N> &object;
    K key;

    CProperty(CObject<N> &objectIn, const K keyIn)
    : object(objectIn), key(keyIn)
    { }

    template <typename V>
    operator V() const
    {
        std::vector<V> values(1);
        V value = values[0];
        object.db.GetProperty(object, key, value);
        return value;
    }
    
    template <typename V>
    CProperty operator=(V value) const
    {
        object.db.SetProperty(object, key, value);
        return *this;
    }

    bool Exists()
    {
        return object.db.HasProperty(object, key);
    }
};

template <typename K, typename N>
class CLocation
{
public:
    CObject<N> &object;
    K key;

    CLocation(CObject<N> &objectIn, K keyIn)
    : object(objectIn), key(keyIn)
    { }

    template <typename V>
    operator V() const
    {
        std::vector<V> values(1);
        V value = values[0];
        object.db.GetLocation(object, key, value);
        return value;
    }
    
    template <typename V>
    CLocation operator=(V value) const
    {
        object.db.SetLocation(object, key, value);
        return *this;
    }

};


#endif