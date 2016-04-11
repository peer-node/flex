#include "credits/MinedCreditMessage.h"
#include "crypto/shorthash.h"

using namespace std;

#include "log.h"
#define LOG_CATEGORY "MinedCreditMessage.cpp"



/************************
 *  MinedCreditMessage
 */

    string_t MinedCreditMessage::ToString() const
    {
        ShortHashList<uint160> list = hash_list;
        list.RecoverFullHashes();
        stringstream ss;
        ss << "\n============== Mined Credit Message =============" << "\n"
        << "== Batch Number: " << mined_credit.batch_number << "\n"
        << "== Credit Hash: " << mined_credit.GetHash160().ToString() << "\n"
        << "==" << "\n"
        << "== Msg Hash: " << GetHash160().ToString() << "\n"
        << "==" << "\n"
        << "== Hash List" << "\n";
        for (uint32_t i = 0; i < list.short_hashes.size(); i++)
        {
            if (i < list.full_hashes.size())
            {
                uint160 hash = list.full_hashes[i];
                string_t type = msgdata[hash]["type"];
                ss << "== " << hash.ToString() << "  " << type
                << "  (" << list.short_hashes[i] << ")\n";
            }
            else
            {
                ss << "== unknown " <<  "(" << list.short_hashes[i] << ")\n";
            }
        }
        ss << "==" << "\n"
        << "== Timestamp: " << timestamp << "\n"
        << "============ End Mined Credit Message ===========" << "\n";
        return ss.str();
    }

    uint256 MinedCreditMessage::GetHash() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    uint160 MinedCreditMessage::GetHash160() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }

    bool MinedCreditMessage::operator==(const MinedCreditMessage& other)
    {
        return GetHash160() == other.GetHash160();
    }

    bool MinedCreditMessage::HaveData()
    {
        return hash_list.RecoverFullHashes();
    }

    bool MinedCreditMessage::CheckHashList()
    {
        hash_list.RecoverFullHashes();

        return hash_list.GetHash160() == mined_credit.hash_list_hash;
    }

/*
 *  MinedCreditMessage
 ************************/