// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef FLEX_CREDITS
#define FLEX_CREDITS

#include "crypto/signature.h"
#include "crypto/hashtrees.h"
#include "crypto/shorthash.h"
#include <boost/foreach.hpp>
#include "mining/work.h"
#include "database/data.h"

#include "log.h"
#define LOG_CATEGORY "credits.h"



#define INITIAL_DIFFICULTY 100000000


template <typename T>
bool VectorContainsEntry(std::vector<T> vector_, T entry)
{
    return find(vector_.begin(), vector_.end(), entry) != vector_.end();
}

template <typename T>
void EraseEntryFromVector(T entry, std::vector<T>& vector_)
{
    uint32_t position = std::distance(vector_.begin(), 
                            std::find(vector_.begin(), vector_.end(), entry));
    if (position < vector_.size())
    {
        vector_.erase(vector_.begin() + position);
    }
}

string_t ByteString(vch_t bytes);

class Credit
{
public:
    uint64_t amount;
    vch_t keydata;

    Credit() { }

    Credit(vch_t keydata, uint64_t amount);

    Credit(vch_t data);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(keydata);
    )

    string_t ToString() const;

    uint160 KeyHash() const;

    vch_t getvch() const;

    bool operator==(const Credit& other_credit) const;

    bool operator<(const Credit& other_credit) const;
};

class MinedCredit : public Credit
{
public:
    uint160 previous_mined_credit_hash;
    uint160 previous_diurn_root;
    uint160 diurnal_block_root;
    uint160 hash_list_hash;
    uint160 batch_root;
    uint160 spent_chain_hash;
    uint160 relay_state_hash;
    uint160 total_work;
    uint32_t batch_number;
    uint32_t offset;
    uint64_t timestamp;
    uint160 difficulty;
    uint160 diurnal_difficulty;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(keydata);
        READWRITE(previous_mined_credit_hash);
        READWRITE(previous_diurn_root);
        READWRITE(diurnal_block_root);
        READWRITE(hash_list_hash);
        READWRITE(batch_root);
        READWRITE(spent_chain_hash);
        READWRITE(relay_state_hash);
        READWRITE(total_work);
        READWRITE(batch_number);
        READWRITE(offset);
        READWRITE(timestamp);
        READWRITE(difficulty);
        READWRITE(diurnal_difficulty);
    )

    string_t ToString() const;

    uint256 GetHash();

    uint160 BranchBridge() const;

    uint160 GetHash160() const;
};


class MinedCreditMessage
{
public:
    MinedCredit mined_credit;
    TwistWorkProof proof;
    ShortHashList<uint160> hash_list;
    uint64_t timestamp;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit);
        READWRITE(proof);
        READWRITE(hash_list);
        READWRITE(timestamp);
    )

    uint256 GetHash() const;

    uint160 GetHash160() const;

    bool operator==(const MinedCreditMessage& other);

    bool HaveData();

    bool CheckHashList();

    bool Validate();

    string_t ToString() const;
};


class CreditInBatch : public Credit
{
public:
    uint64_t position;
    std::vector<uint160> branch;
    std::vector<uint160> diurn_branch;

    CreditInBatch() {}

    CreditInBatch(Credit credit, uint64_t position, 
                  std::vector<uint160> branch);

    string_t ToString() const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(keydata);
        READWRITE(position);
        READWRITE(branch);
        READWRITE(diurn_branch);
    )   

    vch_t getvch();

    bool operator==(const CreditInBatch& other);

    bool operator<(const CreditInBatch& other) const;
};


class CreditBatch
{
public:
    uint160 previous_credit_hash;
    std::vector<Credit> credits;
    OrderedHashTree tree;
    std::vector<vch_t> serialized_credits;

    CreditBatch();

    CreditBatch(uint160 previous_credit_hash, uint64_t offset);

    CreditBatch(const CreditBatch& other);

    string_t ToString() const;

    bool Add(Credit credit);

    bool AddCredits(std::vector<Credit> credits_);

    uint160 Root();

    int32_t Position(Credit credit);

    Credit GetCredit(uint32_t position);

    CreditInBatch GetWithPosition(Credit credit);

    std::vector<uint160> Branch(Credit credit);
};


class UnsignedTransaction
{
public:
    std::vector<CreditInBatch> inputs;
    std::vector<Credit> outputs;
    std::vector<Point> pubkeys;
    bool up_to_date;
    uint256 hash;

    UnsignedTransaction();

    string_t ToString() const;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(inputs);
        READWRITE(outputs);
        READWRITE(pubkeys);
    )

    void AddInput(CreditInBatch batch_credit, bool is_keyhash);

    bool AddOutput(Credit credit);

    uint256 GetHash();

    bool operator!();
};


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

    std::vector<uint160> GetMissingCredits();

    uint64_t IncludedFees();
};

#endif