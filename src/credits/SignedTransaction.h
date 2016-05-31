#ifndef FLEX_SIGNEDTRANSACTION_H
#define FLEX_SIGNEDTRANSACTION_H
#include "../../test/flex_tests/flex_data/TestData.h"
#include "crypto/uint256.h"
#include "vector_tools.h"
#include "crypto/signature.h"
#include "UnsignedTransaction.h"

#include "log.h"
#define LOG_CATEGORY "SignedTransaction.h"


class SignedTransaction
{
public:
    UnsignedTransaction rawtx;
    Signature signature;

    string_t ToString() const;

    static string_t Type() { return string_t("tx"); }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(rawtx);
        READWRITE(signature);
    )

    uint256 GetHash() const;

    uint160 GetHash160() const;

    bool Validate();

    uint64_t IncludedFees();

    std::vector<uint160> Dependencies() { return std::vector<uint160>(); }

    std::vector<uint160> GetMissingCredits(MemoryDataStore &creditdata);

    bool operator==(const SignedTransaction& other) const
    {
        return rawtx == other.rawtx and signature == other.signature;
    }
};


#endif //FLEX_SIGNEDTRANSACTION_H
