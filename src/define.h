// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.

#ifndef TELEPORT_DEFINE
#define TELEPORT_DEFINE

#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <jsoncpp/json/json.h>
#include "base/version.h"
#include "crypto/hash.h"


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

#define HASH160()                                               \
    uint160 GetHash160() const                                  \
    {                                                           \
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);            \
        ss << *this;                                            \
        return Hash160(ss.begin(), ss.end());                   \
    }

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
    void Sign(Data data)                                        \
    {                                                           \
        CBigNum privkey = SigningKey(data);                     \
        signature = Signature(0, 0);                            \
        uint256 hash = GetHash();                               \
        signature = SignHashWithKey(hash, privkey, SECP256K1);  \
    }                                                           \
                                                                \
    bool VerifySignature(Data data)                             \
    {                                                           \
        Point pubkey = VerificationKey(data);                   \
        if (pubkey.IsAtInfinity())                              \
            return false;                                       \
        Signature stored_signature = signature;                 \
        signature = Signature(0, 0);                            \
        uint256 hash = GetHash();                               \
        signature = stored_signature;                           \
        return VerifySignatureOfHash(signature, hash, pubkey);  \
    }                                                           \
                                                                \
    CBigNum SigningKey(Data data)                               \
    {                                                           \
        Point pubkey = VerificationKey(data);                   \
        CBigNum privkey = data.keydata[pubkey]["privkey"];      \
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


// jsonification - allow up to 20 member fields
#define _SELECT_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, NAME, ...) NAME

#define _ADD(X) _json_add(#X, X);
#define _JSON1(X) _ADD(X)
#define _JSON2(X1, ...) _ADD(X1) _JSON1(__VA_ARGS__)
#define _JSON3(X1, ...) _ADD(X1) _JSON2(__VA_ARGS__)
#define _JSON4(X1, ...) _ADD(X1) _JSON3(__VA_ARGS__)
#define _JSON5(X1, ...) _ADD(X1) _JSON4(__VA_ARGS__)
#define _JSON6(X1, ...) _ADD(X1) _JSON5(__VA_ARGS__)
#define _JSON7(X1, ...) _ADD(X1) _JSON6(__VA_ARGS__)
#define _JSON8(X1, ...) _ADD(X1) _JSON7(__VA_ARGS__)
#define _JSON9(X1, ...) _ADD(X1) _JSON8(__VA_ARGS__)
#define _JSON10(X1, ...) _ADD(X1) _JSON9(__VA_ARGS__)
#define _JSON11(X1, ...) _ADD(X1) _JSON10(__VA_ARGS__)
#define _JSON12(X1, ...) _ADD(X1) _JSON11(__VA_ARGS__)
#define _JSON13(X1, ...) _ADD(X1) _JSON12(__VA_ARGS__)
#define _JSON14(X1, ...) _ADD(X1) _JSON13(__VA_ARGS__)
#define _JSON15(X1, ...) _ADD(X1) _JSON14(__VA_ARGS__)
#define _JSON16(X1, ...) _ADD(X1) _JSON15(__VA_ARGS__)
#define _JSON17(X1, ...) _ADD(X1) _JSON16(__VA_ARGS__)
#define _JSON18(X1, ...) _ADD(X1) _JSON17(__VA_ARGS__)
#define _JSON19(X1, ...) _ADD(X1) _JSON18(__VA_ARGS__)
#define _JSON20(X1, ...) _ADD(X1) _JSON19(__VA_ARGS__)

#define _JSON(...) _SELECT_NTH_ARG(__VA_ARGS__, \
        _JSON20, _JSON19, _JSON18, _JSON17, _JSON16, \
        _JSON15, _JSON14, _JSON13, _JSON12, _JSON11, \
        _JSON10, _JSON9, _JSON8, _JSON7, _JSON6, _JSON5, \
        _JSON4, _JSON3, _JSON2, _JSON1)\
    (__VA_ARGS__)

#define JSON(...)   \
    std::string _json_serialization;                                        \
    void _json_append(std::string s)                                        \
    {                                                                       \
        if (_json_serialization != "{")                                     \
            _json_serialization += ",";                                     \
        _json_serialization += s;                                           \
    }                                                                       \
    template<typename T> void _json_add(std::string name, T variable)       \
    {                                                                       \
        _json_append("\"" + name + "\": " + _json_format(variable));        \
    }                                                                       \
    template<typename T> std::string _json_format(T variable)               \
    {                                                                       \
        return variable.json();                                             \
    }                                                                       \
    std::string _json_format(std::string s)                                 \
    {                                                                       \
        return "\"" + s + "\"";                                             \
    }                                                                       \
    std::string _json_format(bool n)                                        \
    { std::stringstream ss; ss << n; return ss.str(); }                     \
    std::string _json_format(uint8_t n)                                     \
    { std::stringstream ss; ss << int(n); return ss.str(); }                \
    std::string _json_format(int64_t n)                                     \
    { std::stringstream ss; ss << n; return ss.str(); }                     \
    std::string _json_format(uint64_t n)                                    \
    { std::stringstream ss; ss << n; return ss.str(); }                     \
    std::string _json_format(int32_t n)                                     \
    { std::stringstream ss; ss << n; return ss.str(); }                     \
    std::string _json_format(uint32_t n)                                    \
    { std::stringstream ss; ss << n; return ss.str(); }                     \
    std::string _json_format(uint160 hash)                                  \
    {                                                                       \
        return "\"" + hash.ToString() + "\"";                               \
    }                                                                       \
    std::string _json_format(uint256 hash)                                  \
    {                                                                       \
        return "\"" + hash.ToString() + "\"";                               \
    }                                                                       \
    std::string _format(uint8_t byte)                                       \
    {                                                                       \
        char buffer[BUFSIZ];                                                \
        sprintf(buffer, "%02x", byte);                                      \
        return std::string(buffer);                                         \
    }                                                                       \
    std::string _json_format(vch_t bytes)                                   \
    {                                                                       \
        std::string result{"\""};                                           \
        for (auto byte : bytes)                                             \
            result += _format(byte);                                        \
        return result + "\"";                                               \
    }                                                                       \
    template<typename T> std::string _json_format(std::vector<T> entries)   \
    {                                                                       \
        std::string result{"["};                                            \
        for (auto entry : entries)                                          \
            result += _json_format(entry) + ",";                            \
        if (result.size() > 1)                                              \
            result.resize(result.size() - 1);                               \
        return result + "]";                                                \
    }                                                                       \
    template<typename T, std::size_t N>                                     \
    std::string _json_format(std::array<T, N> entries)                      \
    {                                                                       \
        std::string result{"["};                                            \
        for (auto entry : entries)                                          \
            result += _json_format(entry) + ",";                            \
        if (result.size() > 1)                                              \
            result.resize(result.size() - 1);                               \
        return result + "]";                                                \
    }                                                                       \
    template<typename T, typename K>                                        \
    std::string _json_format(std::pair<T, K> pair)                          \
    {                                                                       \
        std::string result{"["};                                            \
        result += _json_format(pair.first) + ","                            \
                                           + _json_format(pair.second);     \
        return result + "]";                                                \
    }                                                                       \
    std::string json()                                                      \
    {                                                                       \
        _json_serialization = "{";                                          \
        _JSON(__VA_ARGS__);                                                 \
        return _json_serialization + "}";                                   \
    }

template<typename T> std::string PrettyPrint(T item)
{
    std::stringstream ss;
    ss << item.json();
    return ss.str();
}

#define MIN_TRANSACTION_SIZE 100
#define MAX_OUTPUTS_PER_TRANSACTION 50

#define RFACTOR 4

#define TELEPORT_WORK_NUMBER_OF_MEGABYTES 4096

#endif
