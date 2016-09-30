#ifndef TELEPORT_SIGNEDTRANSACTION_H
#define TELEPORT_SIGNEDTRANSACTION_H

#include "../../test/teleport_tests/teleport_data/TestData.h"
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

    JSON(rawtx, signature);

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

    std::set<uint64_t> InputPositions()
    {
        std::set<uint64_t> positions;
        for (auto input : rawtx.inputs)
            positions.insert(input.position);
        return positions;
    }

    uint64_t TotalInputAmount();

    uint64_t TotalOutputAmount();
};


#endif //TELEPORT_SIGNEDTRANSACTION_H
