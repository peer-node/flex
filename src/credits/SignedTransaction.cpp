#include "../../test/flex_tests/flex_data/TestData.h"

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
        uint64_t total_in = 0, total_out = 0;
        foreach_(CreditInBatch& credit, rawtx.inputs)
            total_in += credit.amount;

        foreach_(Credit& credit, rawtx.outputs)
            total_out += credit.amount;

        if (total_in > total_out)
            return total_in - total_out;

        return 0;
    }

    bool SignedTransaction::Validate()
    {
        uint64_t total_in = 0, total_out = 0;

        set<uint64_t> input_positions;

        if (rawtx.outputs.size() > MAX_OUTPUTS_PER_TRANSACTION)
            return false;

        if (!VerifyTransactionSignature(*this))
        {
            log_ << "ValidateTransaction(): signature verification failed\n";
            return false;
        }

        foreach_(CreditInBatch& credit, rawtx.inputs)
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

        foreach_(const Credit& credit, rawtx.outputs)
        {
            total_out += credit.amount;
        }

        return total_out > MIN_TRANSACTION_SIZE
               && total_in >= total_out;
    }

    vector<uint160> SignedTransaction::GetMissingCredits(MemoryDataStore& creditdata)
    {
        vector<uint160> missing_credits;

        foreach_(const CreditInBatch& credit, rawtx.inputs)
        {
            uint160 batch_root = credit.branch.back();
            uint160 credit_hash = SymmetricCombine(batch_root,
                                                   credit.diurn_branch[0]);
            if (credit.diurn_branch.size() == 1)
            {
                if (!(creditdata[credit_hash]).HasProperty("mined_credit") &&
                    !(creditdata[credit_hash]).HasProperty("branch"))
                {
                    missing_credits.push_back(credit_hash);
                }
            }
        }
        return missing_credits;
    }

/*
 *  SignedTransaction
 ***********************/
