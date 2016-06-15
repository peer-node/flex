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
    credits_in_diurn.push_back(msg);
    diurnal_block.setvch(vch_t());
}

void Diurn::PopulateDiurnalBlockIfNecessary()
{
    if (diurnal_block.Size() != 0)
        return;
    for (auto msg : credits_in_diurn)
        diurnal_block.Add(msg.mined_credit.GetHash160());
}

uint160 Diurn::Last()
{
    PopulateDiurnalBlockIfNecessary();
    if (diurnal_block.Size() == 0)
        return 0;
    return diurnal_block.Last();
}

uint160 Diurn::First()
{
    PopulateDiurnalBlockIfNecessary();
    if (diurnal_block.Size() == 0)
        return 0;
    return diurnal_block.First();
}

bool Diurn::Contains(uint160 hash)
{
    PopulateDiurnalBlockIfNecessary();
    return diurnal_block.Contains(hash);
}

MinedCredit Diurn::GetMinedCreditByHash(uint160 credit_hash)
{
    PopulateDiurnalBlockIfNecessary();
    auto hashes = diurnal_block.Hashes();
    for (int i = 0; i < hashes.size(); i++)
        if (credit_hash == hashes[i])
            return credits_in_diurn[i].mined_credit;
    return MinedCredit();
}

std::vector<uint160> Diurn::Branch(uint160 credit_hash)
{
    PopulateDiurnalBlockIfNecessary();
    if (!Contains(credit_hash))
        return std::vector<uint160>();

    std::vector<uint160> branch{GetMinedCreditByHash(credit_hash).BranchBridge()};
    auto branch_from_branch_bridge_to_diurnal_block_root = diurnal_block.Branch(credit_hash);
    branch.insert(branch.end(),
                  branch_from_branch_bridge_to_diurnal_block_root.begin(),
                  branch_from_branch_bridge_to_diurnal_block_root.end());
    branch.push_back(previous_diurn_root);
    branch.push_back(Root());
    return branch;
}

uint160 Diurn::BlockRoot()
{
    PopulateDiurnalBlockIfNecessary();
    DiurnalBlock block;
    block.setvch(diurnal_block.getvch());
    return block.Root();
}

uint160 Diurn::Root()
{
    PopulateDiurnalBlockIfNecessary();
    return SymmetricCombine(previous_diurn_root, BlockRoot());
}

bool Diurn::VerifyBranch(uint160 batch_root, std::vector<uint160> branch)
{
    uint160 branch_evaluation = EvaluateBranchWithHash(branch, batch_root);
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
    return credits_in_diurn.size();
}
