#ifndef FLEX_MINEDCREDIT_H
#define FLEX_MINEDCREDIT_H

#include "Credit.h"

#include "log.h"
#define LOG_CATEGORY "MinedCredit.h"



class MinedCredit_ : public Credit
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


#endif //FLEX_MINEDCREDIT_H
