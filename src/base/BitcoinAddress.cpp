#include "BitcoinAddress.h"

bool CBitcoinAddress::Set(const CKeyID &id) {
    SetData(Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS), &id, 20);
    return true;
}

bool CBitcoinAddress::Set(const CScriptID &id) {
    SetData(Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS), &id, 20);
    return true;
}

bool CBitcoinAddress::IsValid() const {
    bool fCorrectSize = vchData.size() == 20;
    bool fKnownVersion = vchVersion == Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS) ||
                         vchVersion == Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    return fCorrectSize && fKnownVersion;
}


bool CBitcoinAddress::GetKeyID(CKeyID &keyID) const {
    if (!IsValid() || vchVersion != Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS))
        return false;
    uint160 id;
    memcpy(&id, &vchData[0], 20);
    keyID = CKeyID(id);
    return true;
}

bool CBitcoinAddress::IsScript() const {
    return IsValid() && vchVersion == Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS);
}

void CBitcoinSecret::SetKey(const CKey& vchSecret) {
    assert(vchSecret.IsValid());
    SetData(Params().Base58Prefix(CChainParams::SECRET_KEY), vchSecret.begin(), vchSecret.size());
    if (vchSecret.IsCompressed())
        vchData.push_back(1);
}

void CBitcoinSecret::SetKey(const CKey& vchSecret,
                            const std::vector<unsigned char> prefix) {
    assert(vchSecret.IsValid());
    SetData(prefix, vchSecret.begin(), vchSecret.size());
    if (vchSecret.IsCompressed())
        vchData.push_back(1);
}

CKey CBitcoinSecret::GetKey() {
    CKey ret;
    ret.Set(&vchData[0], &vchData[32], vchData.size() > 32 && vchData[32] == 1);
    return ret;
}

bool CBitcoinSecret::IsValid() const {
    bool fExpectedFormat = vchData.size() == 32 || (vchData.size() == 33 && vchData[32] == 1);
    bool fCorrectVersion = vchVersion == Params().Base58Prefix(CChainParams::SECRET_KEY);
    return fExpectedFormat && fCorrectVersion;
}

bool CBitcoinSecret::SetString(const char* pszSecret) {
    return CBase58Data::SetString(pszSecret) && IsValid();
}

bool CBitcoinSecret::SetString(const std::string& strSecret) {
    return SetString(strSecret.c_str());
}