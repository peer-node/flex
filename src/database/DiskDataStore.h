#ifndef TELEPORT_DISKDATASTORE
#define TELEPORT_DISKDATASTORE

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
#include <src/base/sync.h>
#include <src/base/util_time.h>
#include "base/util_log.h"
#include "crypto/uint256.h"
#include "crypto/hash.h"


#include "log.h"
#define LOG_CATEGORY "datastore.h"

template <typename T> 
inline vch_t Bytes(T thing)
{
    CDataStream ss(SER_DISK, CLIENT_VERSION);
    ss << thing;
    return vch_t(ss.begin(), ss.end());
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
    string_t strPrefix, strKey, strValue;
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
            if (not it->Valid()) return false;

            strKey = it->key().ToString();
            strValue = it->value().ToString();
            CDataStream ss(&strKey[0], &strKey[0] + strKey.size(), SER_DISK, CLIENT_VERSION);

            std::pair<string_t, K> pairKey;
            ss >> pairKey;
            if (pairKey.first != strPrefix)
                return false;

            key = pairKey.second;
            ss = CDataStream(&strValue[0], &strValue[0] + strValue.size(), SER_DISK, CLIENT_VERSION);
            ss >> value;

            it->Next();
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


class DiskDataStore;

class DiskProperty;

class DiskLocation;


class DiskObject
{ 
public:
    DiskDataStore &db;
    vch_t object_name_bytes;
    CDataStream ss;
    string_t strLocalName;  /* A name that won't go out of scope */
    bool fUsingLocalName;

    template<typename N>
    DiskObject(DiskDataStore &db, const N& objectname)
    : db(db), object_name_bytes(Bytes(objectname)), ss(SER_DISK, CLIENT_VERSION)
    {
        fUsingLocalName = false;
        ss << objectname;
    }

    DiskObject(DiskDataStore &db, const string_t strName, bool fUseLocalName)
    : db(db), object_name_bytes(Bytes(strName)), ss(SER_DISK, CLIENT_VERSION)
    {
        strLocalName = strName;
        fUsingLocalName = fUseLocalName;
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return ss.size();
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        s += ss;
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {

    }

    template <typename K>
    DiskProperty operator [] (K& key);

    DiskProperty operator [] (const char* pchKey);

    template <typename K>
    bool HasProperty(K& key);

    bool HasProperty(const char* key);

    DiskLocation Location(const char* pchKey);

    template <typename K>
    DiskLocation Location(const K& key);
};




class DiskLocationIterator
{
public:
    DiskDataStore *datastore;
    CPrefixDBIterator it;
    vch_t location_name;
    uint32_t location_id{0};

    DiskLocationIterator(){};

    DiskLocationIterator(DiskDataStore *pdsIn, uint32_t location_id);

    template <typename V> void Seek(V location_value);

    template <typename V> void Seek(V location_value, bool fReverse);

    void SeekStart()
    {
        Seek((uint8_t)0);
    }

    void SeekEnd()
    {
        Seek((int64_t)(-1), true);
    }

    template <typename N, typename V> bool GetNextObjectAndLocation(N& objectname, V& location_value);

    template <typename N, typename V> bool GetNextObjectAndLocation(N& objectname, V& location_value, bool fReverse);

    template <typename N, typename V> bool GetPreviousObjectAndLocation(N& objectname, V& location_value);
};

typedef std::vector<unsigned char, secure_allocator<unsigned char> > svch_t;


class DiskDataStore : public CPrefixDB
{
private:
    uint32_t nNextNumber;
    bool encrypted;
    svch_t *passphrase_hash{NULL};
    std::map<uint32_t, Mutex*> location_mutexes;
    Mutex *objects_mutex{NULL};



public:
    Mutex& LocationMutex(uint32_t i)
    {
        if (not location_mutexes.count(i))
            location_mutexes[i] = new Mutex();
        return *location_mutexes[i];
    }

    void DeleteMutexes()
    {
        for (auto item : location_mutexes)
            delete item.second;
    }

    DiskDataStore() {  }

    DiskDataStore(CLevelDBWrapper *pdb, const char *pchPrefix)
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

     ~DiskDataStore()
     {
        if (passphrase_hash != NULL)
            delete passphrase_hash;
         DeleteMutexes();
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
        LOCK(LocationMutex(0));
        if (!Read(std::make_pair('k', key), nNumber))
        {
            nNumber = NextNumber();
            Write(std::make_pair('k', key), nNumber);
            Write(std::make_pair('n', nNumber), key);
        }
        return nNumber;
    }

    void Reset() { }

    template <typename K> bool
    GetKey(uint32_t nNumber, K& key)
    {
        LOCK(LocationMutex(0));
        return Read(std::make_pair('n', nNumber), key);
    }

    template <typename K, typename V, typename N> bool
    SetProperty(const K& key, const N& propertyname, const V& value, bool fSync = false) throw(leveldb_error)
    {
        uint32_t key_id = Id(key);
        uint32_t property_id = Id(propertyname);

        LOCK2(LocationMutex(property_id), LocationMutex(key_id));

        if (encrypted)
        {
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            ssValue << value;
            vch_t bytes(ssValue.begin(), ssValue.end());

            uint256 salt = GetSalt();

            std::pair<uint256, vch_t> encrypted_value = std::make_pair(salt,
                                                                       EncryptWithSalt(bytes, *passphrase_hash, salt));

            return Write(std::make_pair(std::make_pair('p', key_id), property_id), encrypted_value, fSync);
        }
        else
        {
            return Write(std::make_pair(std::make_pair('p', key_id), property_id), value, fSync);
        }
    }

    template <typename K, typename V, typename N> bool
    GetProperty(const K& key, const N& propertyname, V& value) throw(leveldb_error)
    {
        if (encrypted)
        {
            std::pair<uint256, vch_t> encrypted_value;
            if (!Read(std::make_pair(std::make_pair('p', Id(key)), Id(propertyname)), encrypted_value))
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
        {
            return Read(std::make_pair(std::make_pair('p', Id(key)), Id(propertyname)), value);
        }
    }

    template <typename K, typename N> bool
    HasProperty(const K& key, const N& propertyname) throw(leveldb_error)
    {
        return Exists(std::make_pair(std::make_pair('p', Id(key)), Id(propertyname)));
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

    template <typename K, typename L, typename V> bool
    SetLocation(const K& key, const L& location_name, const V& value, bool fSync = false) throw(leveldb_error)
    {
        uint32_t location_id = Id(location_name);
        uint32_t key_id = Id(key);

        LOCK2(LocationMutex(location_id), LocationMutex(key_id));

        Write(std::make_pair(std::make_pair('p', key_id), location_id), value, fSync);
        return Write(std::make_pair(std::make_pair('l', location_id), ReverseBytes(value)), key_id, fSync);
    }

    template <typename K, typename V, typename L> bool
    GetLocation(const K& key, const L& location_name, V& value) throw(leveldb_error)
    {
        bool result = GetProperty(key, location_name, value);
        return result;
    }

    template <typename V, typename L> bool
    RemoveFromLocation(const L& location_name, const V& value)
    {
        uint32_t location_id = Id(location_name);
        uint32_t object_id = GetIdOfObjectAtLocation(value, location_name);

        LOCK(LocationMutex(location_id));

        return Erase(std::make_pair(std::make_pair('l', location_id), ReverseBytes(value))) and
                Erase(std::make_pair(std::make_pair('p', object_id), location_id));
    }

    template <typename K, typename V, typename N> bool
    GetObjectAtLocation(const V& location_value,
                        const N& location_name,
                        K& key) throw(leveldb_error)
    {
        uint32_t keyid = GetIdOfObjectAtLocation(location_value, location_name);
        if (keyid == 0)
            return false;
        return GetKey(keyid, key);
    }

    template <typename V, typename N>
    uint32_t GetIdOfObjectAtLocation(const V& location_value,
                                     const N& location_name) throw(leveldb_error)
    {
        uint32_t keyid;
        if (!Read(std::make_pair(std::make_pair('l', Id(location_name)), ReverseBytes(location_value)), keyid))
            return 0;
        return keyid;
    };

    DiskLocationIterator LocationIterator(const char* location_name)
    {
        return DiskLocationIterator(this, Id(Bytes(string_t(location_name))));
    }

    template <typename L>
    DiskLocationIterator LocationIterator(L location_name)
    {
        return DiskLocationIterator(this, Id(location_name));
    }

    DiskObject Object(const char* pchName)
    {
        return DiskObject(*this, string_t(pchName), true);
    }

    DiskObject operator [] (const char* pchName)
    {
        return Object(pchName);
    }

    template <typename N>
    DiskObject Object(const N& objectname)
    {
        return DiskObject(*this, objectname);
    }

    template <typename N>
    DiskObject operator [] (const N& objectname)
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

        CPrefixDBIterator it = Iterator();
        it.Seek(std::make_pair(std::make_pair('p', nObject_id), 0));

        std::pair<std::pair<char, uint32_t>, uint32_t> pairKey;
        char x;
        while (it.GetNextKeyAndValue(pairKey, x))
        {
            if (pairKey.first != std::make_pair('p', nObject_id))
            {
                break;
            }

            try
            {
                uint32_t nProperty_id = pairKey.second;
                Erase(std::make_pair(std::make_pair('p', nObject_id), nProperty_id));
            } catch(...) { }

            // if this property is a location, remove object from it
            uint32_t nLocation_id = pairKey.second;
            string_t strVal = it.strValue;
            std::reverse(strVal.begin(), strVal.end());
            CFlatData value(&strVal[0], &strVal[0] + strVal.size());
            uint32_t nObject_at_location_id = 0;

            if (Read(std::make_pair(std::make_pair('l', nLocation_id), value), nObject_at_location_id) and
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


template <typename K>
bool DiskObject::HasProperty(K& key)
{
    return db.HasProperty(Bytes(*this), key);
}

inline bool DiskObject::HasProperty(const char* key)
{
    return db.HasProperty(Bytes(*this), string_t(key));
}

inline DiskLocationIterator::DiskLocationIterator(DiskDataStore *pdsIn, uint32_t location_id):
    location_id(location_id)
{
    datastore = pdsIn;
    it = datastore->Iterator();
    it.Seek(std::make_pair('l', location_id));
}

template <typename N, typename V> bool
DiskLocationIterator::GetNextObjectAndLocation(N& objectname, V& location_value, bool fReverse)
{
    boost::shared_ptr<leveldb::Iterator> pit = it.it;
    try
    {
        if (!pit->Valid())
            return false;
        std::pair<string_t, std::pair<std::pair<char,uint32_t>,V> > pairKey;
        string_t strKey = pit->key().ToString();
        string_t strValue = pit->value().ToString();
        if (fReverse)
            pit->Prev();
        else
            pit->Next();

        if (strKey.find('l') == std::string::npos)
            return false;

        CDataStream ss(&strKey[0], &strKey[0] + strKey.size(), SER_DISK, CLIENT_VERSION);

        ss >> pairKey;

        if (pairKey.first != it.strPrefix || pairKey.second.first != std::make_pair('l', location_id))
            return false;

        location_value = datastore->ReverseBytes(pairKey.second.second);

        uint32_t nObjectId;
        ss = CDataStream(&strValue[0], &strValue[0] + strValue.size(), SER_DISK, CLIENT_VERSION);
        ss >> nObjectId;
        datastore->GetKey(nObjectId, objectname);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

template <typename N, typename V> bool
DiskLocationIterator::GetPreviousObjectAndLocation(N& objectname, V& location_value)
{
    return GetNextObjectAndLocation(objectname, location_value, true);
}

template <typename N, typename V> bool
DiskLocationIterator::GetNextObjectAndLocation(N& objectname, V& location_value)
{
    return GetNextObjectAndLocation(objectname, location_value, false);
}


template <typename V> void
DiskLocationIterator::Seek(V location_value, bool fReverse)
{
    it.Seek(std::make_pair(std::make_pair('l', location_id), datastore->ReverseBytes(location_value)));
    if (fReverse && it.it->Valid())
    {
        it.it->Prev();
    }
}

template <typename V> void
DiskLocationIterator::Seek(V location_value)
{
    Seek(location_value, false);
}

class DiskProperty
{
public:
    DiskObject *object{NULL};
    vch_t serialized_property_name;

    DiskProperty() { }

    template<typename K>
    DiskProperty(DiskObject &objectIn, const K property_name)
    : object(&objectIn), serialized_property_name(Bytes(property_name))
    { }

    template <typename V>
    operator V() const
    {
        std::vector<V> values(1);
        V value = values[0];
        object->db.GetProperty(*object, serialized_property_name, value);
        return value;
    }
    
    template <typename V>
    DiskProperty operator=(V value) const
    {
        object->db.SetProperty(*object, serialized_property_name, value);
        return *this;
    }

    bool Exists()
    {
        return object->db.HasProperty(*object, serialized_property_name);
    }
};

inline DiskProperty DiskObject::operator [] (const char* pchKey)
{
    return DiskProperty(*this, string_t(pchKey));
}

template <typename K>
DiskProperty DiskObject::operator [] (K& key)
{
    return DiskProperty(*this, key);
}

class DiskLocation
{
public:
    DiskObject *object;
    vch_t serialized_location_name;

    DiskLocation() { }

    template<typename K>
    DiskLocation(DiskObject &objectIn, K location_name)
    : object(&objectIn), serialized_location_name(Bytes(location_name))
    { }

    template <typename V>
    operator V() const
    {
        std::vector<V> values(1);
        V value = values[0];
        object->db.GetLocation(*object, serialized_location_name, value);
        return value;
    }
    
    template <typename V>
    DiskLocation operator=(V value) const
    {
        object->db.SetLocation(*object, serialized_location_name, value);
        return *this;
    }

};

inline DiskLocation DiskObject::Location(const char* pchKey)
{
    return DiskLocation(*this, string_t(pchKey));
}

template <typename K>
DiskLocation DiskObject::Location(const K& key)
{
    return DiskLocation(*this, key);
}

#endif // TELEPORT_DISKDATASTORE