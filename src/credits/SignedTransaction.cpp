#include "../../test/teleport_tests/teleport_data/TestData.h"

#include <boost/foreach.hpp>
#include "define.h"
#include "crypto/hashtrees.h"
#include "SignedTransaction.h"
#include "creditsign.h"

using namespace std;


/***********************
 *  SignedTransaction
 */

    string_t SignedTransaction::ToString() const
    {
        std::stringstream ss;
        ss << "\n============== SignedTransaction =============" << "\n"
        << "== Unsigned Transaction: " << rawtx.ToString()
        << "==" << "\n"
        << "== Signature: " << signature.ToString()
        << "== " << "\n"
        << "== Hash160: " << GetHash160().ToString() << "\n"
        << "============ End SignedTransaction ===========" << "\n";

        return ss.str();
    }

    uint256 SignedTransaction::GetHash() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    uint160 SignedTransaction::GetHash160() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }

    uint64_t SignedTransaction::IncludedFees()
    {
        int64_t excess_input = (int64_t) TotalInputAmount() - (int64_t) TotalOutputAmount();
        return excess_input > 0 ? (uint64_t) excess_input : 0;
    }

    bool SignedTransaction::Validate()
    {
        uint64_t total_in = 0, total_out = 0;

        set<uint64_t> input_positions;

        if (rawtx.outputs.size() > MAX_OUTPUTS_PER_TRANSACTION)
            return false;

        if (not VerifyTransactionSignature(*this))
        {
            log_ << "ValidateTransaction(): signature verification failed\n";
            return false;
        }

        for (auto credit : rawtx.inputs)
        {
            log_ << "ValidateTransaction(): considering credit with position "
                 << credit.position << "\n";

            if (input_positions.count(credit.position))
            {
                log_ << "doublespend! position " << credit.position << "\n";
                return false;
            }
            else
            {
                total_in += credit.amount;
                log_ << "adding " << credit.position
                     << " to spent input positions\n";
                input_positions.insert(credit.position);
            }
        }

        for (auto credit : rawtx.outputs)
        {
            total_out += credit.amount;
        }

        return total_out > MIN_TRANSACTION_SIZE and total_in >= total_out;
    }

    vector<uint160> SignedTransaction::GetMissingCredits(MemoryDataStore& creditdata)
    {
        vector<uint160> missing_credits;

        for (auto credit : rawtx.inputs)
        {
            uint160 batch_root = credit.branch.back();
            uint160 credit_hash = SymmetricCombine(batch_root, credit.diurn_branch[0]);
            if (credit.diurn_branch.size() == 1)
            {
                if (not creditdata[credit_hash].HasProperty("mined_credit") and
                    not creditdata[credit_hash].HasProperty("branch"))
                {
                    missing_credits.push_back(credit_hash);
                }
            }
        }
        return missing_credits;
    }

    uint64_t SignedTransaction::TotalInputAmount()
    {
        uint64_t total = 0;
        for (auto input : rawtx.inputs)
            total += input.amount;
        return total;
    }

    uint64_t SignedTransaction::TotalOutputAmount()
    {
        uint64_t total = 0;
        for (auto output : rawtx.outputs)
            total += output.amount;
        return total;
    }


/*
 *  SignedTransaction
 ***********************/
