#ifndef FLEX_MEMDB
#define FLEX_MEMDB


#include <stdint.h>
#include "base/serialize.h"
#include "base/version.h"
#include "define.h"
#include <map>
#include <stdio.h>

class CMemoryDB;

template <typename K>
class CMemoryDatum
{
public:
	CMemoryDB *pdb;
	K key;

	CMemoryDatum(CMemoryDB *pdbIn, K &keyIn): pdb(pdbIn), key(keyIn)
	{ }

	template <typename V>
    operator V() const;
    
    template <typename V>
    CMemoryDatum operator=(V value) const;
};

class CMemoryDB
{
public:
    std::map<vch_t, vch_t> memdb;

    template <typename K, typename V> void Write(const K& key, const V& value)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << key;
        vch_t vchKey(ss.begin(), ss.end());
        CDataStream ssv(SER_NETWORK, CLIENT_VERSION);
        ssv << value;
        vch_t vchValue(ssv.begin(), ssv.end());
        memdb[vchKey] = vchValue;
    }
    
    template <typename K, typename V> bool Read(const K& key, V& value)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << key;
        vch_t vchKey(ss.begin(), ss.end());
        if (!memdb.count(vchKey))
            return false;
        vch_t vchValue = memdb[vchKey];
        CDataStream ssv(vchValue, SER_NETWORK, CLIENT_VERSION);
        try
        {
            ssv >> value;
            return true;
        } 
        catch (...)
        {
            return false;
        }
    }

    template <typename K> void Erase(const K& key)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << key;
        vch_t vchKey(ss.begin(), ss.end());
        memdb.erase(vchKey);
    }
    
    template <typename K> bool Exists(const K& key)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << key;
        vch_t vchKey(ss.begin(), ss.end());
        return memdb.count(vchKey);
    }

    template <typename K>
    CMemoryDatum<K> operator [] (K key) 
    {
        return CMemoryDatum<K>(this, key);
    }
};

template <typename K>
template <typename V>
CMemoryDatum<K>::operator V() const
{
    std::vector<V> values(1);
    pdb->Read(key, values[0]);
    return values[0];
}

template <typename K>
template <typename V>
CMemoryDatum<K> CMemoryDatum<K>::operator=(V value) const
{
    pdb->Write(key, value);
    return *this;
}

#endif