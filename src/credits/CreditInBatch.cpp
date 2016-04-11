#include "CreditInBatch.h"

#include <boost/foreach.hpp>
#include "crypto/uint256.h"
#include "define.h"

#include "log.h"
#define LOG_CATEGORY "CreditInBatch.cpp"

using namespace std;


/*******************
 *  CreditInBatch
 */

    CreditInBatch::CreditInBatch(Credit credit, uint64_t position,
                                 std::vector<uint160> branch):
        position(position),
        branch(branch)
    {
        keydata = credit.keydata;
        amount = credit.amount;
    }

    string_t CreditInBatch::ToString() const
    {
        stringstream ss;
        ss << "\n============== CreditInBatch =============" << "\n"
        << "== Position: " << position << "\n"
        << "== \n"
        << "== Branch:\n";
                foreach_(uint160 hash, branch)
                        ss << "== " << hash.ToString() << "\n";
        ss << "== \n"
        << "== Diurn Branch:\n";
                foreach_(uint160 hash, diurn_branch)
                        ss << "== " << hash.ToString() << "\n";
        ss << "==" << "\n"
        << "== Credit: " << Credit(keydata, amount).ToString() << "\n"
        << "============ End CreditInBatch ===========" << "\n";
        return ss.str();
    }

    vch_t CreditInBatch::getvch()
    {
        vch_t result;
        result.resize(12 + keydata.size() + 20 * branch.size());

        memcpy(&result[0], &amount, 8);
        memcpy(&result[8], &keydata[0], keydata.size());
        memcpy(&result[8 + keydata.size()], &position, 4);
        memcpy(&result[12 + keydata.size()], &branch[0], 20 * branch.size());

        return result;
    }

    bool CreditInBatch::operator==(const CreditInBatch& other)
    {
        return branch == other.branch && position == other.position;
    }

    bool CreditInBatch::operator<(const CreditInBatch& other) const
    {
        return amount < other.amount;
    }

/*
 *  CreditInBatch
 *******************/
