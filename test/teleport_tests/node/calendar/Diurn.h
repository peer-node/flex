#ifndef TELEPORT_DIURN_H
#define TELEPORT_DIURN_H


#include <src/crypto/hashtrees.h>
#include "test/teleport_tests/node/credit_handler/MinedCreditMessage.h"

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

    JSON(previous_diurn_root, credits_in_diurn);

    HASH160();

    bool operator==(const Diurn& other) const
    {
        return previous_diurn_root == other.previous_diurn_root and credits_in_diurn == other.credits_in_diurn;
    }

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


#endif //TELEPORT_DIURN_H
