#include "Diurn.h"


Diurn::Diurn(const Diurn& diurn)
{
    previous_diurn_root = diurn.previous_diurn_root;
    diurnal_block.setvch(diurn.diurnal_block.getvch());
    credits_in_diurn = diurn.credits_in_diurn;
}

Diurn& Diurn::operator=(const Diurn& diurn)
{
    previous_diurn_root = diurn.previous_diurn_root;
    diurnal_block.setvch(diurn.diurnal_block.getvch());
    credits_in_diurn = diurn.credits_in_diurn;
    return *this;
}

void Diurn::Add(MinedCreditMessage &msg)
{
    diurnal_block.Add(msg.mined_credit.GetHash160());
    credits_in_diurn.push_back(msg);
}

uint160 Diurn::Last()
{
    if (diurnal_block.Size() == 0)
        return 0;
    return diurnal_block.Last();
}

uint160 Diurn::First()
{
    if (diurnal_block.Size() == 0)
        return 0;
    return diurnal_block.First();
}

bool Diurn::Contains(uint160 hash)
{
    return diurnal_block.Contains(hash);
}

std::vector<uint160> Diurn::Branch(uint160 credit_hash)
{
    if (!Contains(credit_hash))
    {
        return std::vector<uint160>();
    }
    std::vector<uint160> branch = diurnal_block.Branch(credit_hash);
    branch.push_back(previous_diurn_root);
    branch.push_back(Root());
    return branch;
}

uint160 Diurn::BlockRoot() const
{
    DiurnalBlock block;
    block.setvch(diurnal_block.getvch());
    return block.Root();
}

uint160 Diurn::Root() const
{
    return SymmetricCombine(previous_diurn_root, BlockRoot());
}

bool Diurn::VerifyBranch(uint160 credit_hash, std::vector<uint160> branch)
{
    uint160 branch_evaluation = EvaluateBranchWithHash(branch, credit_hash);
    return branch_evaluation == branch.back();
}

uint160 Diurn::Work()
{
    uint160 work = 0;
    for (auto msg : credits_in_diurn)
        work += msg.mined_credit.network_state.difficulty;
    return work;
}

uint64_t Diurn::Size()
{
    return diurnal_block.Size();
}







