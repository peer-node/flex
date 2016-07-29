// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef FLEX_DEFINE
#define FLEX_DEFINE

#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>


class MissingDataException : public std::runtime_error
{
public:
    explicit MissingDataException(const std::string& msg)
      : std::runtime_error(msg)
    { }
};

class WrongBatchException : public std::runtime_error
{
public:
    explicit WrongBatchException(const std::string& msg)
      : std::runtime_error(msg)
    { }
};

class NotImplementedException : public std::runtime_error
{
public:
    explicit NotImplementedException(const std::string& msg)
      : std::runtime_error(msg)
    { }
};


template <typename T>
std::string to_string(const T& value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}


typedef std::vector<unsigned char> vch_t;
typedef std::string string_t;

#define foreach_ BOOST_FOREACH

#define IMPLEMENT_HASH_SIGN_VERIFY()                            \
    uint160 GetHash160() const                                  \
    {                                                           \
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);          \
        ss << *this;                                            \
        return Hash160(ss.begin(), ss.end());                   \
    }                                                           \
                                                                \
    uint256 GetHash() const                                     \
    {                                                           \
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);          \
        ss << *this;                                            \
        return Hash(ss.begin(), ss.end());                      \
    }                                                           \
                                                                \
    void Sign()                                                 \
    {                                                           \
        CBigNum privkey = SigningKey();                         \
        signature = Signature(0, 0);                            \
        uint256 hash = GetHash();                               \
        signature = SignHashWithKey(hash, privkey, SECP256K1);  \
    }                                                           \
                                                                \
    bool VerifySignature()                                      \
    {                                                           \
        Point pubkey = VerificationKey();                       \
        if (pubkey.IsAtInfinity())                              \
            return false;                                       \
        Signature stored_signature = signature;                 \
        signature = Signature(0, 0);                            \
        uint256 hash = GetHash();                               \
        signature = stored_signature;                           \
        return VerifySignatureOfHash(signature, hash, pubkey);  \
    }                                                           \
                                                                \
    CBigNum SigningKey()                                        \
    {                                                           \
        Point pubkey = VerificationKey();                       \
        CBigNum privkey = keydata[pubkey]["privkey"];           \
        return privkey;                                         \
    }



#define DEPENDENCIES(...)                                       \
    std::vector<uint160> Dependencies()                         \
    {                                                           \
        std::vector<uint160> result;                            \
        uint160 dependencies[] = { __VA_ARGS__ };               \
        if (sizeof(dependencies) == 0)                          \
            return result;                                      \
        uint32_t length = sizeof(dependencies)                  \
                            / sizeof(dependencies[0]);          \
        for (uint32_t i = 0; i < length; i++)                   \
            result.push_back(dependencies[i]);                  \
        return result;                                          \
    }              

#define HASH160()                                               \
    uint160 GetHash160()                                        \
    {                                                           \
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);            \
        ss << *this;                                            \
        return Hash160(ss.begin(), ss.end());                   \
    }

#define RELAY_FEE 1000000

#define MIN_TRANSACTION_SIZE 100
#define MAX_OUTPUTS_PER_TRANSACTION 50

#define RFACTOR 4


#endif
