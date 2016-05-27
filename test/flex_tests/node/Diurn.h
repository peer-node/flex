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
        READWRITE(credits_in_diurn);
    )

    std::vector<uint160> Branch(uint160 credit_hash);

    static bool VerifyBranch(uint160 credit_hash, std::vector<uint160> branch);

    bool Contains(uint160 hash);

    uint160 BlockRoot();

    uint160 Root();

    uint160 Work();

    uint64_t Size();

    Diurn &operator=(const Diurn &diurn);

    void PopulateDiurnalBlockIfNecessary();

    MinedCredit GetMinedCreditByHash(uint160 credit_hash);
};


#endif //FLEX_DIURN_H
