#include "base/version.h"
#include "crypto/hashtrees.h"
#include "credits/MinedCredit.h"

#include "log.h"
#define LOG_CATEGORY "MinedCredit.cpp"

using namespace std;


/*****************
 *  MinedCredit
 */

    string_t MinedCredit_::ToString() const
    {
        stringstream ss;
        ss << "\n============== Mined Credit =============" << "\n"
        << "== Batch Number: " << batch_number << "\n"
        << "== Credit Hash: " << GetHash160().ToString() << "\n"
        << "== " << "\n"
        << "== Previous Credit Hash: "
        << previous_mined_credit_hash.ToString() << "\n"
        << "== Previous Diurn Root: "
        << previous_diurn_root.ToString() << "\n"
        << "== Batch Root: " << batch_root.ToString() << "\n"
        << "== Spent Chain Hash: " << spent_chain_hash.ToString() << "\n"
        << "== Relay State Hash: " << relay_state_hash.ToString() << "\n"
        << "== Total Work: " << total_work.ToString() << "\n"
        << "== Offset: " << offset << "\n"
        << "== Difficulty: " << difficulty.ToString() << "\n"
        << "== Diurnal Difficulty: "
        << diurnal_difficulty.ToString() << "\n"
        << "==" << "\n"
        << "== Timestamp: " << timestamp << "\n"
        << "============ End Mined Credit ===========" << "\n";
        return ss.str();
    }

    uint256 MinedCredit_::GetHash()
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    uint160 MinedCredit_::BranchBridge() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }

    uint160 MinedCredit_::GetHash160() const
    {
        return SymmetricCombine(BranchBridge(), batch_root);
    }

/*
 *  MinedCredit
 *****************/