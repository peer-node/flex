#ifndef FLEX_DIURN_H
#define FLEX_DIURN_H


#include <src/crypto/hashtrees.h>
#include "MinedCreditMessage.h"

class Diurn
{
public:
    uint160 previous_diurn_root{0};
    DiurnalBlock diurnal_block;
    std::vector<MinedCreditMessage> credits_in_diurn;

    Diurn() { }

    Diurn(const Diurn &diurn);

    void Add(MinedCreditMessage &msg);

    uint160 Last();

    uint160 First();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(previous_diurn_root);
        READWRITE(diurnal_block);
        READWRITE(credits_in_diurn);
    )

    std::vector<uint160> Branch(uint160 credit_hash);

    static bool VerifyBranch(uint160 credit_hash, std::vector<uint160> branch);

    bool Contains(uint160 hash);

    uint160 BlockRoot() const;

    uint160 Root() const;

    uint160 Work();

    uint64_t Size();

    Diurn &operator=(const Diurn &diurn);
};


#endif //FLEX_DIURN_H
