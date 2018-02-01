#include "UnsignedTransaction.h"
#include <boost/foreach.hpp>
#include "vector_tools.h"


/*************************
 *  UnsignedTransaction
 */

    UnsignedTransaction::UnsignedTransaction():
        up_to_date(false),
        hash(0)
    { }

    string_t UnsignedTransaction::ToString() const
    {
        std::stringstream ss;
        ss << "\n============== UnsignedTransaction =============" << "\n"
           << "== Inputs: \n";
        foreach_(CreditInBatch credit, inputs)
            ss << "== " << credit.ToString() << "\n";
        ss << "==" << "\n"
           << "== Outputs: \n";
        foreach_(Credit credit, outputs)
            ss << "== " << credit.ToString() << "\n";
        ss << "==" << "\n"
           << "== Pubkeys: \n";
        foreach_(Point pubkey, pubkeys)
            ss << "== " << pubkey.ToString() << "\n";
        ss << "==" << "\n"
           << "============ End UnsignedTransaction ===========" << "\n";

        return ss.str();
    }

    void UnsignedTransaction::AddInput(CreditInBatch batch_credit, bool is_keyhash)
    {
        inputs.push_back(batch_credit);
        up_to_date = false;
    }

    bool UnsignedTransaction::AddOutput(Credit credit)
    {
        if (VectorContainsEntry(outputs, credit))
            return false;
        outputs.push_back(credit);
        up_to_date = false;
        return true;
    }

    uint256 UnsignedTransaction::GetHash()
    {
        if (up_to_date)
            return hash;
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        hash = Hash(ss.begin(), ss.end());
        up_to_date = true;
        return hash;
    }

    bool UnsignedTransaction::operator!()
    {
        return outputs.size() == 0;
    }

/*
 *  UnsignedTransaction
 *************************/

