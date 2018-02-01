#include <src/vector_tools.h>
#include "CreditBatch.h"



extern string_t ByteString(vch_t bytes);

/*****************
 *  CreditBatch
 */

    CreditBatch::CreditBatch()
    {
        tree.offset = 0ULL;
    }

    CreditBatch::CreditBatch(uint160 previous_credit_hash, uint64_t offset)
            : previous_credit_hash(previous_credit_hash)
    {
        tree.offset = offset;
    }

    CreditBatch::CreditBatch(const CreditBatch& other)
    {
        previous_credit_hash = other.previous_credit_hash;
        tree.offset = other.tree.offset;
        for (auto credit : other.credits)
            Add(credit);
    }

    CreditBatch& CreditBatch::operator=(const CreditBatch& other)
    {
        previous_credit_hash = other.previous_credit_hash;
        tree.offset = other.tree.offset;
        for (auto credit : other.credits)
            Add(credit);
        return *this;
    }

    uint64_t CreditBatch::size()
    {
        return credits.size();
    }

    string_t CreditBatch::ToString() const
    {
        uint160 root = CreditBatch(*this).Root();
        std::stringstream ss;
        ss << "\n============== Credit Batch =============" << "\n"
           << "== Previous Credit Hash: "
           << previous_credit_hash.ToString() << "\n"
           << "== Offset: " << tree.offset << "\n"
           << "==" << "\n"
           << "== Credits:" << "\n";
        for (auto credit : credits)
        {
            ss << "== " <<  credit.TeleportAddress() << " " << credit.amount << "\n";
        }
        ss << "== " << "\n"
            << "== Root: " << root.ToString() << "\n"
            << "============ End Credit Batch ===========" << "\n";
        return ss.str();
    }

    bool CreditBatch::Add(Credit credit)
    {
        credits.push_back(credit);
        vch_t serialized_credit = credit.getvch();
        if (VectorContainsEntry(serialized_credits, serialized_credit))
        {
            log_ << "CreditBatch::Add: serialized_credit already in batch!\n";
            return false;
        }
        serialized_credits.push_back(serialized_credit);
        tree.AddElement(serialized_credits.back());
        return true;
    }

    bool CreditBatch::AddCredits(std::vector<Credit> credits_)
    {
        for (auto credit : credits_)
        {
            vch_t serialized_credit = credit.getvch();
            if (VectorContainsEntry(serialized_credits, serialized_credit))
                return false;
        }
        for (auto credit : credits_)
            Add(credit);
        return true;
    }

    uint160 CreditBatch::Root()
    {
        uint160 treeroot = tree.Root();
        uint160 root = SymmetricCombine(previous_credit_hash, treeroot);
        return root;
    }

    int32_t CreditBatch::Position(Credit credit)
    {
        return tree.Position(credit.getvch());
    }

    Credit CreditBatch::GetCredit(uint32_t position)
    {
        return Credit(tree.GetData(position));
    }

    CreditInBatch CreditBatch::GetCreditInBatch(Credit credit)
    {
        return CreditInBatch(credit, Position(credit), Branch(credit));
    }

    std::vector<uint160> CreditBatch::Branch(Credit credit)
    {
        std::vector<uint160> branch = tree.Branch(Position(credit));
        Credit raw_credit(credit.public_key, credit.amount);
        vch_t raw_credit_data = raw_credit.getvch();

        uint160 hash = OrderedDataElement(Position(credit), raw_credit_data).Hash();

        for (uint32_t i = 0; i < branch.size(); i++)
            hash = SymmetricCombine(hash, branch[i]);

        branch.push_back(previous_credit_hash);
        branch.push_back(Root());
        return branch;
    }

/*
 *  CreditBatch
 *****************/
